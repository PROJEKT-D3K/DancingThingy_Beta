#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "BeatClock.h"
#include "CharacterManager.h"
#include "DancerRenderer.h"

#include <cmath>

namespace dt
{

// One character renderer draws the whole transparent ARGB canvas. The method
// mirrors DancerRenderer::drawDancer's contract so that OverlayWindow's
// render path is agnostic to which artwork is active.
class CharacterRenderer
{
public:
    virtual ~CharacterRenderer() = default;

    virtual void render (juce::Graphics& g, int w, int h, const BeatState& bs,
                         double nowMs, bool mouseHeld) = 0;
};

// The built-in procedural vestal (v0.1 artwork, no files needed).
class ProceduralCharacter : public CharacterRenderer
{
public:
    void setPatternLock (int v) { patternLock = v; }

    void render (juce::Graphics& g, int w, int h, const BeatState& bs,
                 double nowMs, bool mouseHeld) override
    {
        juce::ignoreUnused (mouseHeld);
        drawDancer (g, w, h, bs, nowMs, patternLock);
    }

private:
    int patternLock = -1;
};

// Fruity Dance-compatible sprite-sheet character: 8 uniform cells per row,
// rows named by the sidecar .txt. Row conventions:
//   - "Held"   : shown (animated, wall clock) while the character is dragged
//   - "Waiting": idle loop shown whenever the transport is NOT playing; it is
//                excluded from auto-rotation and from the dance-row menu
//   - all other rows are dance steps cycled every two bars while playing
// A sheet without a Waiting row simply freezes its last frame when idle.
class SpriteCharacter : public CharacterRenderer
{
public:
    static constexpr int framesPerRow = 8;   // fixed by the Fruity Dance format

    explicit SpriteCharacter (CharacterManager::Sheet&& s) : sheet (std::move (s))
    {
        classifyRows();

        contentBounds = computeContentBounds();

        juce::Logger::writeToLog ("[Character] '" + sheet.displayName + "' content "
                                  + juce::String (contentBounds.getWidth()) + "x"
                                  + juce::String (contentBounds.getHeight())
                                  + " at " + juce::String (contentBounds.getX()) + ","
                                  + juce::String (contentBounds.getY())
                                  + " (cell " + juce::String (sheet.cellW) + "x"
                                  + juce::String (sheet.cellH) + ")");
    }

    bool getCellSize (int& cwOut, int& chOut) const
    {
        cwOut = sheet.cellW;
        chOut = sheet.cellH;
        return sheet.ok;
    }

    // Bounding box of the actual artwork inside one cell, unioned over every
    // frame. The overlay sizes its window to this instead of the full cell, so
    // the interactive area hugs the character (see OverlaySession notes on the
    // layered-window hit test).
    bool getContentSize (int& wOut, int& hOut) const
    {
        wOut = contentBounds.getWidth();
        hOut = contentBounds.getHeight();
        return sheet.ok && wOut > 0 && hOut > 0;
    }

    const juce::Rectangle<int>& getContentBounds() const { return contentBounds; }

    int getHeldRow()    const { return heldRow; }
    int getWaitingRow() const { return waitingRow; }

    const juce::String& getDisplayName() const { return sheet.displayName; }

    // dance rows exclude both the "Held" and "Waiting" rows
    int getDanceRowCount() const { return danceIndices.size(); }

    const juce::String& getDanceRowName (int k) const
    {
        static const juce::String empty {};
        const int idx = (k >= 0 && k < danceIndices.size())
                            ? danceIndices.getReference (k) : -1;
        return (idx >= 0) ? sheet.rowNames.getReference (idx) : empty;
    }

    void setRowLock (int v) { rowLock = v; }

    // Changing the multiplier mid-flight would otherwise remap the absolute
    // frame position and make the character jump; setSpeed flags a re-base so
    // the next render keeps the current phase and only continues at the new
    // rate (see applySpeedOffset).
    void setSpeed (double v)
    {
        if (speed != v)
        {
            speed = v;
            speedDirty = true;
        }
    }

    void setMirror (bool v) { mirror = v; }
    void setBlend (bool v)  { blend  = v; }
    void resetFrame()       { lastFrame = 0; lastRow = 0; lastBpm = 0.0; }

    void render (juce::Graphics& g, int w, int h, const BeatState& bs,
                 double nowMs, bool mouseHeld) override
    {
        if (! sheet.ok || sheet.image.isNull() || w <= 0 || h <= 0)
            return;

        const int danceRows = danceIndices.size();
        if (danceRows <= 0 && waitingRow < 0 && heldRow < 0)
            return;

        if (bs.playing && bs.bpm > 0.0)
            lastBpm = bs.bpm;                       // remembered for idle pacing
        const double bpm = (lastBpm > 0.0 ? lastBpm : 100.0);

        int    row   = lastRow;
        int    frame = lastFrame;
        float  frac  = 0.0f;
        double rawF  = 0.0;
        bool   hasFrames = false;

        if (mouseHeld && heldRow >= 0)
        {
            // held: loop this row on the wall clock
            row  = heldRow;
            rawF = (nowMs * 0.001) * (bpm / 60.0) * 2.0 * speed;
            hasFrames = true;
        }
        else if (! bs.playing && waitingRow >= 0)
        {
            // idle: loop the Waiting sequence on the wall clock
            row  = waitingRow;
            rawF = (nowMs * 0.001) * (bpm / 60.0) * 2.0 * speed;
            hasFrames = true;
        }
        else if (bs.playing)
        {
            // baseline: 8 frames per bar, i.e. one cell per eighth note
            // (FL's spec fixes 8 cells per row; the per-beat pacing that the
            // FL plugin applies to those cells is not documented, so this
            // baseline is our adjustable default via the speed multiplier).
            // Frame position derives from transport state, so it never
            // drifts with the refresh rate or the wall clock.
            rawF = (bs.beatInBar + bs.beatPhase) * 2.0 * speed;
            hasFrames = true;

            if (danceRows > 0)
            {
                int rowIdx;
                if (rowLock >= 0 && rowLock < danceRows)
                {
                    rowIdx = rowLock;
                }
                else
                {
                    // auto: every two bars move to the next dance row
                    // ("Waiting" and "Held" are not part of the cycle; the
                    // bar-boundary switch is built in, barIndex is constant
                    // within a bar).
                    const long long m = (((bs.barIndex / 2) % danceRows) + danceRows) % danceRows;
                    rowIdx = (int) m;
                }
                row = danceIndices.getReference (rowIdx);
            }
            else
            {
                row = (waitingRow >= 0 ? waitingRow : 0);
            }
        }
        // else: no Waiting row on the sheet -> freeze the last frame

        if (hasFrames)
        {
            const double f = applySpeedOffset (rawF);
            frame = wrapFrame (f);
            frac  = (float) (f - std::floor (f));
        }

        lastRow   = row;
        lastFrame = frame;

        drawFrame (g, w, h, row, frame);

        if (blend && frac > 0.001f)
        {
            const int next = (frame + 1) % framesPerRow;
            g.setOpacity (frac);
            drawFrame (g, w, h, row, next);
            g.setOpacity (1.0f);
        }
    }

private:
    // Keeps the visible phase continuous when `speed` changes: re-basing the
    // offset so rawF + offset reproduces the previous frame position exactly
    // on the first frame at the new rate.
    double applySpeedOffset (double rawF)
    {
        if (speedDirty)
        {
            speedPhaseOffset = lastF - rawF;
            speedDirty = false;
        }

        const double f = rawF + speedPhaseOffset;
        lastF = f;
        return f;
    }

    static int wrapFrame (double f)
    {
        const int m = (int) std::floor (f) % framesPerRow;
        return m < 0 ? m + framesPerRow : m;
    }

    void classifyRows()
    {
        heldRow = waitingRow = -1;
        danceIndices.clear();

        for (int i = 0; i < sheet.rowNames.size(); ++i)
        {
            const auto name = sheet.rowNames [i].trim().toLowerCase();

            if (name == "held" && heldRow < 0)
                heldRow = i;
            else if (name == "waiting" && waitingRow < 0)
                waitingRow = i;
            else
                danceIndices.add (i);
        }

        juce::Logger::writeToLog ("[Character] '" + sheet.displayName
                                  + "' rows: waitingRow=" + juce::String (waitingRow)
                                  + ", heldRow=" + juce::String (heldRow)
                                  + ", danceRows=" + juce::String (danceIndices.size())
                                  + "/" + juce::String (sheet.rowNames.size()));
    }

    void drawFrame (juce::Graphics& g, int w, int h, int row, int col)
    {
        // Scoped: blending draws two frames per call, and a transform left
        // behind by the first would cancel the mirror out on the second.
        const juce::Graphics::ScopedSaveState state (g);

        if (mirror)
            g.addTransform (juce::AffineTransform::scale (-1.0f, 1.0f,
                            (float) w * 0.5f, (float) h * 0.5f));

        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (sheet.image,
                     0, 0, w, h,
                     col * sheet.cellW + contentBounds.getX(),
                     row * sheet.cellH + contentBounds.getY(),
                     contentBounds.getWidth(), contentBounds.getHeight(), false);
    }

    // One pass over the sheet: every opaque-ish pixel contributes its position
    // modulo the cell size, so the union across all frames/rows comes for free.
    juce::Rectangle<int> computeContentBounds() const
    {
        const juce::Rectangle<int> full { 0, 0, juce::jmax (1, sheet.cellW), juce::jmax (1, sheet.cellH) };

        if (! sheet.ok || sheet.image.isNull() || ! sheet.image.isARGB()
            || sheet.cellW <= 0 || sheet.cellH <= 0)
            return full;

        const juce::Image::BitmapData bd (sheet.image, juce::Image::BitmapData::readOnly);

        if (bd.pixelStride != 4)
            return full;

        constexpr int alphaThreshold = 8;

        int minX = sheet.cellW, minY = sheet.cellH, maxX = -1, maxY = -1;

        for (int y = 0; y < bd.height; ++y)
        {
            const auto* pixels = reinterpret_cast<const juce::PixelARGB*> (bd.getLinePointer (y));
            const int cellY = y % sheet.cellH;

            for (int x = 0; x < bd.width; ++x)
            {
                if (pixels[x].getAlpha() <= alphaThreshold)
                    continue;

                const int cellX = x % sheet.cellW;
                minX = juce::jmin (minX, cellX);
                maxX = juce::jmax (maxX, cellX);
                minY = juce::jmin (minY, cellY);
                maxY = juce::jmax (maxY, cellY);
            }
        }

        if (maxX < minX || maxY < minY)
            return full;

        const auto r = juce::Rectangle<int> (minX, minY, maxX - minX + 1, maxY - minY + 1)
                           .getIntersection (full);

        return r.isEmpty() ? full : r;
    }

    CharacterManager::Sheet sheet;
    juce::Array<int> danceIndices;
    juce::Rectangle<int> contentBounds { 0, 0, 1, 1 };
    int    heldRow    = -1;
    int    waitingRow = -1;
    int    rowLock = -1;
    double speed  = 1.0;
    bool   mirror = false;
    bool   blend  = false;
    int    lastFrame = 0;
    int    lastRow   = 0;
    double lastBpm   = 0.0;   // remembered tempo, paces the idle/held loops

    // speed re-basing state (see setSpeed / applySpeedOffset)
    double speedPhaseOffset = 0.0;
    double lastF            = 0.0;
    bool   speedDirty       = false;
};

} // namespace dt

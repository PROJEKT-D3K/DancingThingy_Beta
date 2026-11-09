#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
#include <vector>

namespace dt
{

// ---------------------------------------------------------------------------
// Sprite-sheet character management, modeled on Fruity Dance's skin system:
//
//   - one character = one sprite sheet image + a same-named .txt in the same
//     folder ("Dance.png" + "Dance.txt")
//   - the sheet is exactly 8 cells wide, with 1..N rows; each row of the
//     sheet is one 8-frame animation sequence
//   - each line of the .txt names one row in order; the last row is
//     conventionally called "Held" and plays while the character is dragged
//   - cell pixel size is arbitrary but uniform across the whole sheet
//
// Characters are discovered by scanning folders (roots). The default root is
// %APPDATA%\DanceOverlay\Characters; extra roots can be added at runtime
// (e.g. FL Studio's "...\Plugins\Fruity\Generators\Fruity Dance\Artwork"
// folder), reading the user's own sheets in place without copying anything.
//
// JUCE 8 ships decoders for PNG/JPEG/GIF only (no BMP), so .bmp files are not
// scanned; the Fruity Dance manual itself recommends PNG/GIF for alpha.
// ---------------------------------------------------------------------------
class CharacterManager
{
public:
    struct Entry
    {
        juce::String displayName;      // menu label (sheet base name)
        juce::File   imageFile;
        juce::File   nameFile;         // the same-named .txt listing row names
    };

    struct Sheet
    {
        juce::Image       image;
        juce::StringArray rowNames;    // one per physical row
        juce::String      displayName;
        juce::File        sourceFile;
        int   rows = 0, cellW = 0, cellH = 0;
        bool  ok = false;
        juce::String error;
    };

    CharacterManager()
    {
        charactersDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                            .getChildFile ("DanceOverlay").getChildFile ("Characters");

        if (! charactersDir.exists())
            charactersDir.createDirectory();
    }

    const juce::File& getCharactersDir() const { return charactersDir; }

    void rescan()
    {
        entries.clear();

        juce::Array<juce::File> seen;
        constexpr const char* exts[] { ".png", ".gif", ".jpg", ".jpeg" };

        for (const auto& root : scanRoots())
        {
            if (! root.isDirectory())
                continue;

            for (const auto* ext : exts)
            {
                juce::Array<juce::File> files;
                root.findChildFiles (files, juce::File::findFiles, false, juce::String ("*") + ext);

                for (const auto& f : files)
                {
                    if (seen.contains (f))
                        continue;

                    seen.add (f);

                    Entry e;
                    e.imageFile    = f;
                    e.nameFile     = f.withFileExtension (".txt");
                    e.displayName  = f.getFileNameWithoutExtension();
                    entries.add (e);
                }
            }
        }

        // stable menu order across rescans
        std::vector<Entry> sorted;
        for (int i = 0; i < entries.size(); ++i)
            sorted.push_back (entries.getReference (i));
        std::sort (sorted.begin(), sorted.end(), [] (const Entry& a, const Entry& b)
                   { return a.displayName.compareIgnoreCase (b.displayName) < 0; });
        entries.clear();
        for (auto& e : sorted)
            entries.add (e);

        juce::Logger::writeToLog ("[Character] rescan: " + juce::String (entries.size())
                                  + " sheet(s) accessible");
    }

    int getNumEntries() const { return entries.size(); }

    const Entry& getEntry (int index) const
    {
        jassert (index >= 0 && index < entries.size());
        return entries.getReference (juce::jlimit (0, entries.size() - 1, index));
    }

    bool entryHasTxt (int index) const
    {
        return index >= 0 && index < entries.size()
               && entries.getReference (index).nameFile.existsAsFile();
    }

    // Parses the sheet for display: image + row names -> uniform cell grid.
    Sheet loadSheet (int index) const
    {
        Sheet sh;
        if (index < 0 || index >= entries.size())
        {
            sh.error = "index out of range";
            return sh;
        }

        const auto& e = entries.getReference (index);
        sh.displayName = e.displayName;
        sh.sourceFile  = e.imageFile;

        if (! e.nameFile.existsAsFile())
        {
            sh.error = "missing same-named .txt (Fruity Dance sheets need one row name per txt line)";
            return sh;
        }

        juce::StringArray lines;
        e.nameFile.readLines (lines);

        // some .txt files (FL's own artwork included) carry a UTF-8 BOM on
        // the first line; strip it or "Waiting"/"Held" won't be recognised
        if (lines.size() > 0 && lines [0].startsWithChar (0xFEFF))
            lines.getReference (0) = lines [0].substring (1);

        while (lines.size() > 0 && lines [lines.size() - 1].trim().isEmpty())
            lines.remove (lines.size() - 1);

        if (lines.isEmpty())
        {
            sh.error = "the .txt names no rows";
            return sh;
        }

        sh.image = juce::ImageFileFormat::loadFrom (e.imageFile);
        if (sh.image.isNull())
        {
            sh.error = "cannot decode image (PNG/JPEG/GIF only; re-save BMP sheets as PNG)";
            return sh;
        }

        const int imgW = sh.image.getWidth();
        const int imgH = sh.image.getHeight();
        const int cw   = imgW / 8;
        const int ch   = imgH / lines.size();

        if (cw <= 0 || ch <= 0)
        {
            sh.error = "sheet is too small for an 8-cell grid";
            return sh;
        }

        if (cw * 8 != imgW || ch * lines.size() != imgH)
            juce::Logger::writeToLog ("[Character] '" + sh.displayName
                                      + "' cell grid leaves " + juce::String (imgW - cw * 8)
                                      + "/" + juce::String (imgH - ch * lines.size())
                                      + " trailing pixel(s) unused (w*8=" + juce::String (cw * 8)
                                      + ", h*rows=" + juce::String (ch * lines.size())
                                      + ", img=" + juce::String (imgW) + "x" + juce::String (imgH) + ")");

        juce::Logger::writeToLog ("[Character] load '" + sh.displayName + "': rows="
                                  + juce::String (lines.size()) + ", cell="
                                  + juce::String (cw) + "x" + juce::String (ch)
                                  + " from " + sh.sourceFile.getFullPathName());

        sh.rowNames = lines;
        sh.rows  = lines.size();
        sh.cellW = cw;
        sh.cellH = ch;
        sh.ok    = true;
        return sh;
    }

    void setExtraRoots (const juce::StringArray& folderPaths)
    {
        extraRootPaths.clear();
        for (const auto& p : folderPaths)
            if (p.trim().isNotEmpty())
                extraRootPaths.addIfNotAlreadyThere (p.trim());

        rescan();
    }

    juce::StringArray getExtraRoots() const { return extraRootPaths; }

    void setExtraRootsFromString (const juce::String& newlineJoined)
    {
        juce::StringArray a;
        a.addLines (newlineJoined);
        setExtraRoots (a);
    }

    juce::String getExtraRootsAsString() const { return extraRootPaths.joinIntoString ("\n"); }

private:
    juce::Array<juce::File> scanRoots() const
    {
        juce::Array<juce::File> r;
        r.add (charactersDir);
        for (const auto& p : extraRootPaths)
            r.add (juce::File (p));
        return r;
    }

    juce::File        charactersDir;
    juce::StringArray extraRootPaths;
    juce::Array<Entry> entries;
};

} // namespace dt

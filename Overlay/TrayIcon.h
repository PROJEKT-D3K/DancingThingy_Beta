#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "OverlaySession.h"

namespace dt
{

// System tray icon + menu. Builds the same menu as the session's tray path.
class TrayIconComponent : public juce::SystemTrayIconComponent
{
public:
    explicit TrayIconComponent (OverlaySession& ownerIn) : owner (ownerIn)
    {
        juce::Image icon (juce::Image::ARGB, 16, 16, true);
        {
            juce::Graphics g (icon);
            g.fillAll (juce::Colours::transparentBlack);
            g.setColour (juce::Colour (0xff6FA8DC));
            g.fillEllipse (2.0f, 2.0f, 12.0f, 12.0f);
            g.setColour (juce::Colour (0xffF5D06F));
            g.fillEllipse (6.5f, 6.5f, 3.0f, 3.0f);
        }
        setIconImage (icon, icon);
        refreshTooltip();
    }

    void refreshTooltip()
    {
        setIconTooltip (tr (Text::trayTooltip) + " - " + juce::String (owner.getNumDancers()));
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        const auto at = juce::Desktop::getInstance().getMousePosition();
        owner.showTrayMenu ({ at.x, at.y });
    }

private:
    OverlaySession& owner;
};

} // namespace dt

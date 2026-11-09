#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "OverlayWindow.h"

namespace dt
{

// System tray icon + menu. Mirrors the same menu used on right-click.
class TrayIconComponent : public juce::SystemTrayIconComponent
{
public:
    explicit TrayIconComponent (OverlayWindow& ownerIn) : owner (ownerIn)
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
        setIconTooltip ("DanceOverlay");
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        const auto at = juce::Desktop::getInstance().getMousePosition();
        juce::PopupMenu menu;
        owner.buildTrayMenu (menu);

        menu.showMenuAsync (juce::PopupMenu::Options()
                                .withTargetScreenArea ({ at.x, at.y, 1, 1 }),
                            [safeOwner = juce::Component::SafePointer<OverlayWindow> (&owner)] (int r)
                            {
                                if (safeOwner != nullptr)
                                    safeOwner->handleMenuResult (r);
                            });
    }

private:
    OverlayWindow& owner;
};

} // namespace dt

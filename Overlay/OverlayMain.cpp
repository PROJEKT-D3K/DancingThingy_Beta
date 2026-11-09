#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_data_structures/juce_data_structures.h>

#include "OverlayWindow.h"

namespace dt
{

// FileLogger writes into a buffered FileOutputStream and never flushes, so a
// running overlay's log stays empty until the process exits - useless when the
// thing is diagnosed by tailing that file (or killed with taskkill). Line
// flushing costs nothing at this log volume.
class OverlayLogger : public juce::Logger
{
public:
    OverlayLogger (const juce::File& logFile, const juce::String& welcome)
        : file (logFile)
    {
        file.getParentDirectory().createDirectory();
        stream = file.createOutputStream();

        logMessage (welcome);
        logMessage ("Log started: " + juce::Time::getCurrentTime().toString (false, true));
    }

    void logMessage (const juce::String& message) override
    {
        const juce::ScopedLock sl (lock);

        if (stream != nullptr)
        {
            stream->writeText (message + juce::newLine, false, false, nullptr);
            stream->flush();
        }
    }

private:
    juce::File file;
    std::unique_ptr<juce::FileOutputStream> stream;
    juce::CriticalSection lock;
};

class DanceOverlayApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "DanceOverlay"; }
    const juce::String getApplicationVersion() override { return "0.3.0"; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String&) override
    {
        const auto logFile = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                 .getChildFile ("DanceOverlay")
                                 .getChildFile ("overlay.log");

        fileLogger = std::make_unique<OverlayLogger> (logFile, "DanceOverlay starting");
        juce::Logger::setCurrentLogger (fileLogger.get());

        appProperties = std::make_unique<juce::ApplicationProperties>();

        juce::PropertiesFile::Options opts;
        opts.applicationName = "settings";
        opts.folderName      = "DanceOverlay";
        opts.filenameSuffix  = ".xml";
        opts.storageFormat   = juce::PropertiesFile::storeAsXML;

        appProperties->setStorageParameters (opts);

        window = std::make_unique<OverlayWindow> (*appProperties->getUserSettings());
        window->show();
    }

    void shutdown() override
    {
        if (appProperties != nullptr && window != nullptr)
        {
            window->saveToProperties();
            appProperties->saveIfNeeded();
        }

        window.reset();
        appProperties.reset();
        juce::Logger::setCurrentLogger (nullptr);
        fileLogger.reset();
    }

    void systemRequestedQuit() override { juce::JUCEApplication::quit(); }

    void anotherInstanceStarted (const juce::String&) override {}

private:
    std::unique_ptr<OverlayWindow>                window;
    std::unique_ptr<juce::ApplicationProperties>  appProperties;
    std::unique_ptr<OverlayLogger>                fileLogger;
};

} // namespace dt

START_JUCE_APPLICATION (dt::DanceOverlayApplication)

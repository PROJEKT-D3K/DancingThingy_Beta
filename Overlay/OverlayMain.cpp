#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_data_structures/juce_data_structures.h>

#include "OverlaySession.h"

namespace dt
{

#ifndef DT_VERSION
 #define DT_VERSION "0.0.0"
#endif

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
    const juce::String getApplicationVersion() override { return DT_VERSION; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String& commandLine) override
    {
        const auto logFile = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                 .getChildFile ("DanceOverlay")
                                 .getChildFile ("overlay.log");

        fileLogger = std::make_unique<OverlayLogger> (logFile,
                                                      "DanceOverlay " + getApplicationVersion() + " starting");
        juce::Logger::setCurrentLogger (fileLogger.get());

        appProperties = std::make_unique<juce::ApplicationProperties>();

        juce::PropertiesFile::Options opts;
        opts.applicationName = "settings";
        opts.folderName      = "DanceOverlay";
        opts.filenameSuffix  = ".xml";
        opts.storageFormat   = juce::PropertiesFile::storeAsXML;

        appProperties->setStorageParameters (opts);

        session = std::make_unique<OverlaySession> (*appProperties->getUserSettings());
        session->start();

        // "--settings": open the settings window right away (also handy for a
        // desktop shortcut that goes straight to the configuration).
        if (commandLine.contains ("--settings"))
            session->openSettingsWindow();
    }

    void shutdown() override
    {
        if (session != nullptr)
            session->shutdown();

        session.reset();

        if (appProperties != nullptr)
            appProperties->saveIfNeeded();

        appProperties.reset();
        juce::Logger::setCurrentLogger (nullptr);
        fileLogger.reset();
    }

    void systemRequestedQuit() override { juce::JUCEApplication::quit(); }

    void anotherInstanceStarted (const juce::String&) override {}

private:
    std::unique_ptr<OverlaySession>               session;
    std::unique_ptr<juce::ApplicationProperties>  appProperties;
    std::unique_ptr<OverlayLogger>                fileLogger;
};

} // namespace dt

START_JUCE_APPLICATION (dt::DanceOverlayApplication)

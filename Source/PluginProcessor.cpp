#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace dt
{

// juce::Optional has hasValue()/operator* but no value_or.
static bool   getOrDefault (const juce::Optional<bool>& o, bool d)   { return o.hasValue() ? *o : d; }
static double getOrDefault (const juce::Optional<double>& o, double d) { return o.hasValue() ? *o : d; }

// Sends the latest transport snapshot over local UDP on its own thread, so
// the audio thread never touches networking.
class DanceSenderThread : public juce::Thread
{
public:
    explicit DanceSenderThread (const std::atomic<TransportSnapshot>& sourceToRead)
        : juce::Thread ("DanceBeatSender"), source (sourceToRead)
    {
    }

    void run() override
    {
        juce::DatagramSocket socket;

        while (! threadShouldExit())
        {
            const auto text = makePacket (source.load (std::memory_order_relaxed));
            socket.write ("127.0.0.1", defaultPort,
                          text.toRawUTF8(), (int) text.getNumBytesAsUTF8());
            wait (15);   // ~66 packets/s heartbeat
        }
    }

private:
    const std::atomic<TransportSnapshot>& source;
};

DanceThingyAudioProcessor::DanceThingyAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    sender = std::make_unique<DanceSenderThread> (snapshot);
    sender->startThread (juce::Thread::Priority::low);
}

DanceThingyAudioProcessor::~DanceThingyAudioProcessor()
{
    if (sender != nullptr)
    {
        sender->stopThread (500);
        sender.reset();
    }
}

void DanceThingyAudioProcessor::prepareToPlay (double, int) {}
void DanceThingyAudioProcessor::releaseResources() {}

bool DanceThingyAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return ! in.isDisabled() && ! out.isDisabled() && in == out;
}

void DanceThingyAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ignoreUnused (buffer);   // straight passthrough, samples forwarded untouched

    TransportSnapshot s;
    s.timestampMs = juce::Time::getMillisecondCounter();

    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
        {
            s.valid       = true;
            s.isPlaying   = getOrDefault (position->getIsPlaying(), false);
            s.bpm         = getOrDefault (position->getBpm(), 0.0);
            s.ppqPosition = getOrDefault (position->getPpqPosition(), -1.0);
            s.barStartPpq = getOrDefault (position->getPpqPositionOfLastBarStart(), -1.0);

            if (s.ppqPosition < 0.0)   // some hosts provide no musical position
                s.valid = false;
        }

    snapshot.store (s, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* DanceThingyAudioProcessor::createEditor()
{
    return new DanceThingyAudioProcessorEditor (*this);
}

} // namespace dt

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new dt::DanceThingyAudioProcessor();
}

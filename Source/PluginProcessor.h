#pragma once
#include <JuceHeader.h>
#include "ClipDSP.h"

class ClipPocketAudioProcessor final : public juce::AudioProcessor {
public:
 ClipPocketAudioProcessor();
 void prepareToPlay(double,int) override;
 void releaseResources() override {}
 void reset() override;
 bool isBusesLayoutSupported(const BusesLayout&) const override;
 using juce::AudioProcessor::processBlock;
 using juce::AudioProcessor::processBlockBypassed;
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 void processBlock(juce::AudioBuffer<double>&,juce::MidiBuffer&) override;
 bool supportsDoublePrecisionProcessing() const override {return true;}
 void processBlockBypassed(juce::AudioBuffer<float>& b,juce::MidiBuffer& m) override {processAudio(b,m,true);}
 void processBlockBypassed(juce::AudioBuffer<double>& b,juce::MidiBuffer& m) override {processAudio(b,m,true);}
 juce::AudioProcessorEditor* createEditor() override;
 juce::AudioProcessorParameter* getBypassParameter() const override {return parameters.getParameter("bypass");}
 bool hasEditor() const override {return true;}
 const juce::String getName() const override {return JucePlugin_Name;}
 bool acceptsMidi() const override {return false;}
 bool producesMidi() const override {return false;}
 bool isMidiEffect() const override {return false;}
 double getTailLengthSeconds() const override {return .5;} // Allow correction filters to decay.
 int getNumPrograms() override {return 1;}
 int getCurrentProgram() override {return 0;}
 void setCurrentProgram(int) override {}
 const juce::String getProgramName(int) override {return {};}
 void changeProgramName(int,const juce::String&) override {}
 void getStateInformation(juce::MemoryBlock&) override;
 void setStateInformation(const void*,int) override;
 static juce::AudioProcessorValueTreeState::ParameterLayout layout();
 void setParameterValue(const char*,float);
 juce::AudioProcessorValueTreeState parameters;
 std::atomic<float> meterIn{0},meterOut{0},meterGR{0};
 std::atomic<bool> displayBypass{false};
 std::atomic<int> editorWidth{800};
private:
 clip::Engine engine;
 std::array<std::atomic<float>*,5> knobs{};
 std::array<std::atomic<float>*,4> options{};
 clip::Settings settings() const noexcept;
 template<typename Sample> void processAudio(juce::AudioBuffer<Sample>&,juce::MidiBuffer&,bool);
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipPocketAudioProcessor)
};

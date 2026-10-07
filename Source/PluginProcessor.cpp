#include "PluginProcessor.h"
#include "PluginEditor.h"
namespace {
constexpr const char* knobIds[]{"in","ceiling","output","bass"};
constexpr const char* optionIds[]{"quality","bypass","renderHQ"};
void peakStore(std::atomic<float>& destination,float value) noexcept {
 float old=destination.load(std::memory_order_relaxed);while(old<value&&!destination.compare_exchange_weak(old,value,std::memory_order_relaxed)){}
}
}
ClipPocketAudioProcessor::ClipPocketAudioProcessor()
 :AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),parameters(*this,nullptr,"ClipPocketState",layout()){
 for(size_t i=0;i<knobs.size();++i)knobs[i]=parameters.getRawParameterValue(knobIds[i]);
 for(size_t i=0;i<options.size();++i)options[i]=parameters.getRawParameterValue(optionIds[i]);
 engine.prepare(48000.);setLatencySamples(engine.latency());
}
juce::AudioProcessorValueTreeState::ParameterLayout ClipPocketAudioProcessor::layout(){
 juce::AudioProcessorValueTreeState::ParameterLayout result;
 auto add=[&](const char* id,const char* name,float lo,float hi,float value,const char* unit,float skew=1.f){
  result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name,juce::NormalisableRange<float>{lo,hi,.01f,skew},value,juce::AudioParameterFloatAttributes().withLabel(unit)));};
 add("in","Input Gain",-24,36,0,"dB");add("ceiling","Ceiling",-30,0,0,"dB");add("output","Output Gain",-36,24,0,"dB");
 add("bass","Low Protect",0,100,0,"%");
 result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"quality",1},"Quality",juce::StringArray{"Eco 2x","Studio 4x","Master 8x","Ultra 16x","Extreme 32x","Offline 64x"},3));
 result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"renderHQ",1},"Maximum quality on offline render",true));
 result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass",1},"Bypass",false));
 result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"link",1},"Input Output Link",false));
 return result;
}

clip::Settings ClipPocketAudioProcessor::settings() const noexcept {
 clip::Settings s;double* dest[]{&s.inDb,&s.ceilingDb,&s.outDb,&s.bass};
 for(size_t i=0;i<knobs.size();++i)*dest[i]=knobs[i]->load(std::memory_order_relaxed);
 s.mode=0;s.knee=0.;s.quality=juce::roundToInt(options[0]->load());s.bypass=options[1]->load()>.5f;
 if(options[2]->load()>.5f&&isNonRealtime())s.quality=5;
 return s;
}
void ClipPocketAudioProcessor::prepareToPlay(double sr,int){engine.prepare(sr);setLatencySamples(engine.latency());meterIn=0;meterOut=0;meterGR=0;}
void ClipPocketAudioProcessor::reset(){engine.reset();meterIn=0;meterOut=0;meterGR=0;}
bool ClipPocketAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
 return l.inputBuses.size()==1&&l.outputBuses.size()==1&&l.getMainInputChannelSet()==l.getMainOutputChannelSet()&&(l.getMainOutputChannelSet()==juce::AudioChannelSet::mono()||l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo());
}
void ClipPocketAudioProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer& m){processAudio(b,m,false);}
void ClipPocketAudioProcessor::processBlock(juce::AudioBuffer<double>& b,juce::MidiBuffer& m){processAudio(b,m,false);}
template<typename Sample> void ClipPocketAudioProcessor::processAudio(juce::AudioBuffer<Sample>& b,juce::MidiBuffer& m,bool hostBypass){
 juce::ScopedNoDenormals noDenormals;m.clear();auto s=settings();s.bypass=s.bypass||hostBypass;displayBypass.store(s.bypass,std::memory_order_relaxed);
 const int channels=getTotalNumInputChannels();for(int c=channels;c<b.getNumChannels();++c)b.clear(c,0,b.getNumSamples());if(channels<1||b.getNumSamples()==0)return;
 auto* left=b.getWritePointer(0);auto* right=channels>1?b.getWritePointer(1):nullptr;float in=0,out=0,gr=0;
 for(int n=0;n<b.getNumSamples();++n){const auto y=engine.process(left[n],right?right[n]:left[n],s,channels);left[n]=static_cast<Sample>(y.l);if(right)right[n]=static_cast<Sample>(y.r);
  in=std::max(in,static_cast<float>(y.inputPeak));out=std::max({out,static_cast<float>(std::abs(left[n])),right?static_cast<float>(std::abs(right[n])):0.f});gr=std::max(gr,static_cast<float>(y.reductionDb));}
 peakStore(meterIn,in);peakStore(meterOut,out);peakStore(meterGR,gr);
}
void ClipPocketAudioProcessor::setParameterValue(const char* id,float v){if(auto* p=parameters.getParameter(id)){p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(v));p->endChangeGesture();}}
void ClipPocketAudioProcessor::setLinkedGain(const char* id,float value){
 if(parameters.getRawParameterValue("link")->load()<.5f){setParameterValue(id,value);return;}
 const bool isInput=juce::String(id)=="in";
 value=juce::jlimit(isInput?-24.f:-36.f,isInput?36.f:24.f,value);
 setParameterValue(id,value);setParameterValue(isInput?"output":"in",-value);
}
void ClipPocketAudioProcessor::getStateInformation(juce::MemoryBlock& b){auto state=parameters.copyState();state.setProperty("version",6,nullptr);if(auto xml=state.createXml())copyXmlToBinary(*xml,b);}
void ClipPocketAudioProcessor::setStateInformation(const void* data,int bytes){
 if(auto xml=getXmlFromBinary(data,bytes);xml&&xml->hasTagName(parameters.state.getType())){
  auto state=juce::ValueTree::fromXml(*xml);if(!state.isValid())return;
  // Keep existing IN/Ceiling/Output/bass/quality state, but retired controls
  // cannot turn the deleted limiter or creative processing back on.
  for(int i=state.getNumChildren()-1;i>=0;--i){auto child=state.getChild(i);const auto id=child.getProperty("id").toString();if(parameters.getParameter(id)==nullptr)state.removeChild(i,nullptr);}
  if(!state.getChildWithProperty("id","link").isValid()){juce::ValueTree link("PARAM");link.setProperty("id","link",nullptr);link.setProperty("value",0,nullptr);state.addChild(link,-1,nullptr);}
  state.setProperty("version",6,nullptr);parameters.replaceState(state);
 }
}
juce::AudioProcessorEditor* ClipPocketAudioProcessor::createEditor(){return new ClipPocketAudioProcessorEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new ClipPocketAudioProcessor();}

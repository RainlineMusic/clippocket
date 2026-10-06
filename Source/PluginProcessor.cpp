#include "PluginProcessor.h"
#include "PluginEditor.h"
namespace {
constexpr const char* knobIds[]{"in","ceiling","output","shape","clean","focus","punch","bass","texture","link","asymmetry","phase","release","mix"};
constexpr const char* optionIds[]{"quality","mode","dc","isp","autoGain","delta","bypass","renderHQ"};
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
 add("in","IN",-24,36,0,"dB");add("ceiling","Ceiling",-30,0,0,"dB");add("output","Output",-24,12,0,"dB");
 add("shape","Shape",0,100,0,"%");add("clean","Cleanliness",0,100,100,"%");add("focus","Focus",0,100,50,"%");add("punch","Punch",0,100,0,"%");add("bass","Bass Anchor",0,100,0,"%");
 add("texture","Texture",-100,100,0,"%");add("link","Stereo Link",0,100,100,"%");add("asymmetry","Asymmetry",-100,100,0,"%");add("phase","Phase Assist",0,100,0,"%");add("release","Guard Release",1,250,30,"ms",.4f);add("mix","Mix",0,100,100,"%");
 result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"quality",1},"Quality",juce::StringArray{"Eco 2x","Studio 4x","Master 8x","Ultra 16x","Extreme 32x","Offline 64x"},2));
 result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"mode",1},"Model",juce::StringArray{"Clean","Perceptual","Soft"},0));
 result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"dc",1},"DC correction",true));
 result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"isp",1},"ISP Guard",true));
 result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"autoGain",1},"Drive compensation",false));
 result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"delta",1},"Delta listen",false));
 result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"renderHQ",1},"Maximum quality on offline render",true));
 result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass",1},"Bypass",false));return result;
}
clip::Settings ClipPocketAudioProcessor::settings() const noexcept {
 clip::Settings s;double* dest[]{&s.inDb,&s.ceilingDb,&s.outDb,&s.shape,&s.clean,&s.focus,&s.punch,&s.bass,&s.texture,&s.link,&s.asymmetry,&s.phase,&s.releaseMs,&s.mix};
 for(size_t i=0;i<knobs.size();++i)*dest[i]=knobs[i]->load(std::memory_order_relaxed);
 s.quality=juce::roundToInt(options[0]->load());s.mode=juce::roundToInt(options[1]->load());s.dc=options[2]->load()>.5f;s.isp=options[3]->load()>.5f;s.autoGain=options[4]->load()>.5f;s.delta=options[5]->load()>.5f;s.bypass=options[6]->load()>.5f;if(options[7]->load()>.5f&&isNonRealtime())s.quality=5;return s;
}
void ClipPocketAudioProcessor::prepareToPlay(double sr,int){engine.prepare(sr);setLatencySamples(engine.latency());meterIn=0;meterOut=0;meterGR=0;meterGuard=0;}
void ClipPocketAudioProcessor::reset(){engine.reset();meterIn=0;meterOut=0;meterGR=0;meterGuard=0;}
bool ClipPocketAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
 return l.inputBuses.size()==1&&l.outputBuses.size()==1&&l.getMainInputChannelSet()==l.getMainOutputChannelSet()&&(l.getMainOutputChannelSet()==juce::AudioChannelSet::mono()||l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo());
}
void ClipPocketAudioProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer& m){processAudio(b,m,false);}
void ClipPocketAudioProcessor::processBlock(juce::AudioBuffer<double>& b,juce::MidiBuffer& m){processAudio(b,m,false);}
template<typename Sample> void ClipPocketAudioProcessor::processAudio(juce::AudioBuffer<Sample>& b,juce::MidiBuffer& m,bool hostBypass){
 juce::ScopedNoDenormals noDenormals;m.clear();auto s=settings();s.bypass=s.bypass||hostBypass;displayBypass.store(s.bypass,std::memory_order_relaxed);
 const int channels=getTotalNumInputChannels();for(int c=channels;c<b.getNumChannels();++c)b.clear(c,0,b.getNumSamples());if(channels<1||b.getNumSamples()==0)return;
 auto* left=b.getWritePointer(0);auto* right=channels>1?b.getWritePointer(1):nullptr;float in=0,out=0,gr=0,safety=0;
 for(int n=0;n<b.getNumSamples();++n){const auto y=engine.process(left[n],right?right[n]:left[n],s,channels);left[n]=static_cast<Sample>(y.l);if(right)right[n]=static_cast<Sample>(y.r);
  in=std::max(in,static_cast<float>(y.inputPeak));out=std::max({out,static_cast<float>(std::abs(left[n])),right?static_cast<float>(std::abs(right[n])):0.f});gr=std::max(gr,static_cast<float>(y.reductionDb));safety=std::max(safety,static_cast<float>(y.guardDb));}
 peakStore(meterIn,in);peakStore(meterOut,out);peakStore(meterGR,gr);peakStore(meterGuard,safety);
}
void ClipPocketAudioProcessor::setParameterValue(const char* id,float v){if(auto* p=parameters.getParameter(id)){p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(v));p->endChangeGesture();}}
void ClipPocketAudioProcessor::loadPreset(int index){
 const float defaults[]{0,0,0,0,100,50,0,0,0,100,0,0,30,100};for(size_t i=0;i<knobs.size();++i)setParameterValue(knobIds[i],defaults[i]);
 setParameterValue("mode",0);setParameterValue("quality",2);setParameterValue("dc",1);setParameterValue("isp",1);setParameterValue("delta",0);setParameterValue("autoGain",0);setParameterValue("bypass",0);setParameterValue("renderHQ",1);
 switch(index){
 case 1:setParameterValue("in",4);setParameterValue("mode",1);setParameterValue("focus",60);setParameterValue("punch",30);break;
 case 2:setParameterValue("in",6);setParameterValue("shape",8);setParameterValue("release",8);setParameterValue("link",75);break;
 case 3:setParameterValue("in",4);setParameterValue("shape",20);setParameterValue("bass",45);setParameterValue("release",70);break;
 case 4:setParameterValue("mode",2);setParameterValue("in",7);setParameterValue("shape",60);setParameterValue("texture",-30);break;
 case 5:setParameterValue("in",6);setParameterValue("mode",1);setParameterValue("focus",30);setParameterValue("punch",45);setParameterValue("quality",3);setParameterValue("ceiling",-.3f);break;
 default:break;
 }
}
void ClipPocketAudioProcessor::getStateInformation(juce::MemoryBlock& b){auto state=parameters.copyState();state.setProperty("version",1,nullptr);if(auto xml=state.createXml())copyXmlToBinary(*xml,b);}
void ClipPocketAudioProcessor::setStateInformation(const void* data,int bytes){if(auto xml=getXmlFromBinary(data,bytes);xml&&xml->hasTagName(parameters.state.getType())){auto state=juce::ValueTree::fromXml(*xml);if(state.isValid())parameters.replaceState(state);}}
juce::AudioProcessorEditor* ClipPocketAudioProcessor::createEditor(){return new ClipPocketAudioProcessorEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new ClipPocketAudioProcessor();}

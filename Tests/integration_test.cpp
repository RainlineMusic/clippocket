#include "PluginEditor.h"
#include <iostream>
#include <cstdlib>
#include <memory>
#include <cmath>
struct ClipUiTestAccess {
 static void theme(ClipPocketAudioProcessorEditor& e,PocketTheme t){e.setTheme(t,false);}
 static void tick(ClipPocketAudioProcessorEditor& e){e.timerCallback();}
 static bool layout(ClipPocketAudioProcessorEditor& e){
  std::vector<juce::Component*> controls{&e.input,&e.ceiling,&e.output,&e.bass,&e.outMeter,&e.grMeter,&e.settingsButton,&e.bypassButton,&e.linkButton};
  for(auto* c:controls)if(!e.getLocalBounds().contains(c->getBounds()))return false;
  for(size_t i=0;i<controls.size();++i)for(size_t j=i+1;j<controls.size();++j)if(controls[i]->getBounds().intersects(controls[j]->getBounds()))return false;
  return true;
 }
 static void linkControls(ClipPocketAudioProcessorEditor& e){
  e.audioProcessor.setParameterValue("link",1);e.input.setValue(10.,juce::sendNotificationSync);requireReset(std::abs(e.output.getValue()+10.)<1.e-4);e.output.setValue(-6.,juce::sendNotificationSync);requireReset(std::abs(e.input.getValue()-6.)<1.e-4);e.input.setValue(36.,juce::sendNotificationSync);requireReset(std::abs(e.input.getValue()-36.)<1.e-4&&std::abs(e.output.getValue()+36.)<1.e-4);e.audioProcessor.setParameterValue("link",0);e.input.setValue(0.,juce::sendNotificationSync);e.output.setValue(0.,juce::sendNotificationSync);
 }
 static void meterPreview(ClipPocketAudioProcessorEditor& e){e.outMeter.setLevel(clip::dbGain(1.),clip::dbGain(1.));e.grMeter.setLevel(8.f,8.f);}
 static void meterReset(ClipPocketAudioProcessorEditor& e){e.outMeter.setLevel(.8f,.8f);e.grMeter.setLevel(5.f,5.f);requireReset(e.outMeter.peak()>.7f&&e.grMeter.peak()>4.f);e.outMeter.resetPeak();e.grMeter.resetPeak();requireReset(e.outMeter.peak()==0.f&&e.grMeter.peak()==0.f);e.outMeter.setLevel(0.f,0.f);e.grMeter.setLevel(0.f,0.f);}
 static void requireReset(bool ok){if(!ok)std::abort();}
 static bool blurred(ClipPocketAudioProcessorEditor& e){return e.blurredSnapshot.isValid();}
};
static void require(bool b,const char* msg){if(!b){std::cerr<<"FAIL "<<msg<<'\n';std::exit(1);}}
// JUCE's 0.01 step can leave sub-micro-dB residue with ARM fused arithmetic.
// Keep the tolerance much smaller than one user-visible parameter step.
static void requireNear(float actual,float expected,const char* msg){
 if(!std::isfinite(actual)||std::abs(actual-expected)>1.e-5f){
  std::cerr<<"FAIL "<<msg<<": expected "<<expected<<", got "<<actual<<'\n';std::exit(1);
 }
}
static void png(const juce::Image& image,const juce::File& file){juce::FileOutputStream stream(file);require(stream.openedOk(),"PNG file");stream.setPosition(0);stream.truncate();juce::PNGImageFormat encoder;require(encoder.writeImageToStream(image,stream),"PNG encode");}
int main(int argc,char** argv){
 juce::ScopedJuceInitialiser_GUI init;
 auto p=std::make_unique<ClipPocketAudioProcessor>();p->setPlayConfigDetails(2,2,48000.,257);p->prepareToPlay(48000.,257);
 require(p->getLatencySamples()==2080,"declared latency");require(std::abs(p->getTailLengthSeconds()-.5)<1.e-12,"filter tail");requireNear(p->parameters.getRawParameterValue("ceiling")->load(),0.f,"ceiling default");
 requireNear(p->parameters.getRawParameterValue("in")->load(),0.f,"input default");requireNear(p->parameters.getRawParameterValue("output")->load(),0.f,"output default");
 requireNear(p->parameters.getRawParameterValue("link")->load(),0.f,"link off default");
 require(p->getParameters().size()==8,"only eight active parameters");
 const char* stableIds[]{"in","ceiling","output","bass","quality","renderHQ","bypass","link"};
 for(int i=0;i<8;++i)require(dynamic_cast<juce::RangedAudioParameter*>(p->getParameters()[i])->getParameterID()==stableIds[i],"parameter index compatibility");
 for(const char* id:{"mode","knee","delta","mix","isp","release","autoGain","shape","clean","focus","punch","texture","asymmetry","phase"})require(p->parameters.getParameter(id)==nullptr,"retired controls removed");
 p->setParameterValue("in",6);p->setParameterValue("bass",60);juce::AudioBuffer<float> b(2,257);juce::MidiBuffer midi;
 for(int frame=0;frame<18;++frame){for(int i=0;i<257;++i){const auto x=.8f*std::sin(float((frame*257+i)*2.*clip::pi*997./48000.));b.setSample(0,i,x);b.setSample(1,i,x*.7f);}p->processBlock(b,midi);for(int c=0;c<2;++c)for(int i=0;i<257;++i)require(std::isfinite(b.getSample(c,i)),"processor finite");}
 
 juce::MemoryBlock state;p->getStateInformation(state);auto copy=std::make_unique<ClipPocketAudioProcessor>();copy->setStateInformation(state.getData(),int(state.getSize()));
 for(auto* parameter:p->getParameters()){auto* ranged=dynamic_cast<juce::RangedAudioParameter*>(parameter);require(ranged!=nullptr,"ranged parameter");require(std::abs(ranged->getValue()-copy->parameters.getParameter(ranged->getParameterID())->getValue())<1.e-6f,"state roundtrip");}
 p->setParameterValue("in",0);p->setParameterValue("bass",0);p->reset();for(int frame=0;frame<32;++frame){for(int i=0;i<257;++i){const float x=1.7f*std::sin(float((frame*257+i)*2.*clip::pi*997./48000.));b.setSample(0,i,x);b.setSample(1,i,x*.7f);}p->processBlock(b,midi);}
 auto editor=std::unique_ptr<ClipPocketAudioProcessorEditor>(static_cast<ClipPocketAudioProcessorEditor*>(p->createEditor()));
 ClipUiTestAccess::linkControls(*editor);ClipUiTestAccess::meterReset(*editor);ClipUiTestAccess::meterPreview(*editor);
 const auto folder=juce::File(argc>1?argv[1]:"ui-captures");folder.createDirectory();
 int index=0;for(auto theme:{PocketTheme::SolidDark,PocketTheme::Neon,PocketTheme::Amber}){
  ClipUiTestAccess::theme(*editor,theme);
  for(int width:{600,800,1200}){editor->setSize(width,juce::roundToInt(width*520./800.));require(ClipUiTestAccess::layout(*editor),"compact layout");if(width==800){auto snap=editor->createComponentSnapshot(editor->getLocalBounds(),true,2.f);png(snap,folder.getChildFile("theme-"+juce::String(index)+"-2x.png"));}}
  ++index;
 }
 ClipUiTestAccess::theme(*editor,PocketTheme::SolidDark);editor->setSize(800,520);require(ClipUiTestAccess::layout(*editor),"compact layout");png(editor->createComponentSnapshot(editor->getLocalBounds()),folder.getChildFile("compact.png"));
 p->setParameterValue("bypass",1);ClipUiTestAccess::tick(*editor);require(ClipUiTestAccess::blurred(*editor),"bypass blur");png(editor->createComponentSnapshot(editor->getLocalBounds()),folder.getChildFile("bypass.png"));
 for(int i=0;i<20;++i){auto transient=std::unique_ptr<juce::AudioProcessorEditor>(p->createEditor());transient.reset();}
 editor.reset();
 auto mono=std::make_unique<ClipPocketAudioProcessor>();mono->setPlayConfigDetails(1,1,44100.,64);mono->prepareToPlay(44100.,64);juce::AudioBuffer<float> mb(1,64);mb.clear();mono->processBlock(mb,midi);
 p->setParameterValue("bypass",0);p->setParameterValue("bass",40);p->setParameterValue("in",6);p->setParameterValue("renderHQ",1);p->setNonRealtime(true);p->getStateInformation(state);copy->setStateInformation(state.getData(),int(state.getSize()));copy->setParameterValue("quality",5);copy->setNonRealtime(false);p->prepareToPlay(48000.,257);copy->setPlayConfigDetails(2,2,48000.,257);copy->prepareToPlay(48000.,257);juce::AudioBuffer<float> cb(2,257);
 for(int frame=0;frame<24;++frame){for(int i=0;i<257;++i){const float x=.7f*std::sin(float((frame*257+i)*2.*clip::pi*7301./48000.));b.setSample(0,i,x);b.setSample(1,i,x*.4f);}cb.makeCopyOf(b);p->processBlock(b,midi);copy->processBlock(cb,midi);for(int c=0;c<2;++c)for(int i=0;i<257;++i)require(std::abs(b.getSample(c,i)-cb.getSample(c,i))<1.e-6f,"offline maximum quality equivalence");}
 p->setNonRealtime(false);
 p->setParameterValue("in",0);p->setParameterValue("bass",0);p->setProcessingPrecision(juce::AudioProcessor::doublePrecision);p->prepareToPlay(48000.,257);juce::AudioBuffer<double> db(2,257);for(int frame=0;frame<16;++frame){for(int i=0;i<257;++i){db.setSample(0,i,1.7*std::sin((frame*257+i)*2.*clip::pi*7013./48000.));db.setSample(1,i,db.getSample(0,i)*.6);}p->processBlock(db,midi);for(int c=0;c<2;++c)for(int i=0;i<257;++i)require(std::isfinite(db.getSample(c,i)),"double-precision finite output");}
 auto legacy=p->parameters.copyState();legacy.setProperty("version",1,nullptr);
 for(const char* id:{"isp","autoGain","mix","mode","delta","release"}){juce::ValueTree param("PARAM");param.setProperty("id",id,nullptr);param.setProperty("value",1,nullptr);legacy.addChild(param,-1,nullptr);}
 juce::MemoryBlock legacyBlock;juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(),legacyBlock);copy->setStateInformation(legacyBlock.getData(),int(legacyBlock.getSize()));
 for(int i=0;i<copy->parameters.state.getNumChildren();++i)require(copy->parameters.getParameter(copy->parameters.state.getChild(i).getProperty("id").toString())!=nullptr,"legacy state pruned");
 requireNear(copy->parameters.getRawParameterValue("link")->load(),0.f,"legacy link off");
 std::cout<<"PASS processor, 32/64-bit, mono/stereo, 8 parameters, state, latency, 3 themes, 3 sizes, blur, 20 editor lifecycles\n";
}

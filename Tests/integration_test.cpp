#include "PluginEditor.h"
#include <iostream>
#include <cstdlib>
#include <memory>
#include <cmath>
struct ClipUiTestAccess {
 static void theme(ClipPocketAudioProcessorEditor& e,PocketTheme t){e.setTheme(t,false);}
 static void expand(ClipPocketAudioProcessorEditor& e,bool value){e.setExpanded(value,false);}
 static void tick(ClipPocketAudioProcessorEditor& e){e.timerCallback();}
 static bool layout(ClipPocketAudioProcessorEditor& e){
  std::vector<juce::Component*> controls{&e.input,&e.ceiling,&e.output,&e.mix,&e.inMeter,&e.outMeter,&e.grMeter,&e.settingsButton,&e.bypassButton,&e.expandButton,&e.cleanButton,&e.perceptualButton,&e.softButton,&e.deltaButton,&e.autoButton};
  for(auto& c:e.advanced)if(c->isVisible())controls.push_back(c.get());
  for(auto* c:controls)if(!e.getLocalBounds().contains(c->getBounds()))return false;
  for(size_t i=0;i<controls.size();++i)for(size_t j=i+1;j<controls.size();++j)if(controls[i]->getBounds().intersects(controls[j]->getBounds()))return false;
  return true;
 }
 static bool perceptualEnabled(ClipPocketAudioProcessorEditor& e){return e.advanced[1]->isEnabled()&&e.advanced[2]->isEnabled()&&e.advanced[3]->isEnabled();}
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
 require(p->getLatencySamples()==624,"declared latency");require(std::abs(p->getTailLengthSeconds()-.5)<1.e-12,"filter tail");requireNear(p->parameters.getRawParameterValue("ceiling")->load(),0.f,"ceiling default");
 requireNear(p->parameters.getRawParameterValue("in")->load(),0.f,"input default");requireNear(p->parameters.getRawParameterValue("output")->load(),0.f,"output default");
 p->loadPreset(5);juce::AudioBuffer<float> b(2,257);juce::MidiBuffer midi;
 for(int frame=0;frame<18;++frame){for(int i=0;i<257;++i){const auto x=.8f*std::sin(float((frame*257+i)*2.*clip::pi*997./48000.));b.setSample(0,i,x);b.setSample(1,i,x*.7f);}p->processBlock(b,midi);for(int c=0;c<2;++c)for(int i=0;i<257;++i)require(std::isfinite(b.getSample(c,i)),"processor finite");}
 juce::MemoryBlock state;p->getStateInformation(state);auto copy=std::make_unique<ClipPocketAudioProcessor>();copy->setStateInformation(state.getData(),int(state.getSize()));
 for(auto* parameter:p->getParameters()){auto* ranged=dynamic_cast<juce::RangedAudioParameter*>(parameter);require(ranged!=nullptr,"ranged parameter");require(std::abs(ranged->getValue()-copy->parameters.getParameter(ranged->getParameterID())->getValue())<1.e-6f,"state roundtrip");}
 p->loadPreset(0);p->reset();for(int frame=0;frame<32;++frame){for(int i=0;i<257;++i){const float x=1.7f*std::sin(float((frame*257+i)*2.*clip::pi*997./48000.));b.setSample(0,i,x);b.setSample(1,i,x*.7f);}p->processBlock(b,midi);}
 auto editor=std::unique_ptr<ClipPocketAudioProcessorEditor>(static_cast<ClipPocketAudioProcessorEditor*>(p->createEditor()));
 require(!ClipUiTestAccess::perceptualEnabled(*editor),"perceptual knobs disabled in Clean");p->setParameterValue("mode",1);ClipUiTestAccess::tick(*editor);require(ClipUiTestAccess::perceptualEnabled(*editor),"perceptual knobs enabled");p->setParameterValue("mode",0);ClipUiTestAccess::tick(*editor);
 const auto folder=juce::File(argc>1?argv[1]:"ui-captures");folder.createDirectory();
 int index=0;for(auto theme:{PocketTheme::SolidDark,PocketTheme::Neon,PocketTheme::Amber,PocketTheme::SolidWhite}){
  ClipUiTestAccess::theme(*editor,theme);ClipUiTestAccess::expand(*editor,true);
  for(int width:{600,800,1200}){editor->setSize(width,juce::roundToInt(width*850./800.));require(ClipUiTestAccess::layout(*editor),"expanded layout");if(width==800){auto snap=editor->createComponentSnapshot(editor->getLocalBounds(),true,2.f);png(snap,folder.getChildFile("theme-"+juce::String(index)+"-2x.png"));}}
  ++index;
 }
 ClipUiTestAccess::theme(*editor,PocketTheme::SolidDark);editor->setSize(800,850);ClipUiTestAccess::expand(*editor,false);require(ClipUiTestAccess::layout(*editor),"collapsed layout");png(editor->createComponentSnapshot(editor->getLocalBounds()),folder.getChildFile("compact.png"));
 ClipUiTestAccess::expand(*editor,true);p->setParameterValue("bypass",1);ClipUiTestAccess::tick(*editor);require(ClipUiTestAccess::blurred(*editor),"bypass blur");png(editor->createComponentSnapshot(editor->getLocalBounds()),folder.getChildFile("bypass.png"));
 for(int i=0;i<20;++i){auto transient=std::unique_ptr<juce::AudioProcessorEditor>(p->createEditor());transient.reset();}
 editor.reset();
 ClipPocketAudioProcessor mono;mono.setPlayConfigDetails(1,1,44100.,64);mono.prepareToPlay(44100.,64);juce::AudioBuffer<float> mb(1,64);mb.clear();mono.processBlock(mb,midi);
 p->loadPreset(2);p->setParameterValue("renderHQ",1);p->setNonRealtime(true);p->getStateInformation(state);copy->setStateInformation(state.getData(),int(state.getSize()));copy->setParameterValue("quality",5);copy->setNonRealtime(false);p->prepareToPlay(48000.,257);copy->setPlayConfigDetails(2,2,48000.,257);copy->prepareToPlay(48000.,257);juce::AudioBuffer<float> cb(2,257);
 for(int frame=0;frame<24;++frame){for(int i=0;i<257;++i){const float x=.7f*std::sin(float((frame*257+i)*2.*clip::pi*7301./48000.));b.setSample(0,i,x);b.setSample(1,i,x*.4f);}cb.makeCopyOf(b);p->processBlock(b,midi);copy->processBlock(cb,midi);for(int c=0;c<2;++c)for(int i=0;i<257;++i)require(std::abs(b.getSample(c,i)-cb.getSample(c,i))<1.e-6f,"offline maximum quality equivalence");}
 p->setNonRealtime(false);
 p->loadPreset(0);p->setProcessingPrecision(juce::AudioProcessor::doublePrecision);p->prepareToPlay(48000.,257);juce::AudioBuffer<double> db(2,257);for(int frame=0;frame<16;++frame){for(int i=0;i<257;++i){db.setSample(0,i,1.7*std::sin((frame*257+i)*2.*clip::pi*7013./48000.));db.setSample(1,i,db.getSample(0,i)*.6);}p->processBlock(db,midi);for(int c=0;c<2;++c)for(int i=0;i<257;++i)require(std::isfinite(db.getSample(c,i))&&std::abs(db.getSample(c,i))<=1.+1.e-12,"double-precision ceiling");}
 std::cout<<"PASS processor, 32/64-bit, mono/stereo, presets, state, latency, 4 themes, 3 sizes, blur, 20 editor lifecycles\n";
}

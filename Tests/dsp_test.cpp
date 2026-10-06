#include "../Source/ClipDSP.h"
#include <memory>
#include <iostream>
#include <random>
#include <chrono>
#include <cstdlib>
#include <vector>
#include <new>
static bool countAudioAllocations=false;
static int audioAllocations=0;
void* operator new(std::size_t n){if(countAudioAllocations)++audioAllocations;if(auto* p=std::malloc(n))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
static void require(bool b,const char* msg){if(!b){std::cerr<<"FAIL: "<<msg<<'\n';std::exit(1);}}
int main(){
 auto engine=std::make_unique<clip::Engine>();
 for(double rate:{44100.,48000.,88200.,96000.,176400.,192000.}){
  engine->prepare(rate);clip::Settings s;s.dc=false;s.isp=false;
  std::vector<double> history;history.reserve(16000);double error=0.;
  for(int i=0;i<16000;++i){const double x=.2*std::sin(2.*clip::pi*997.*i/rate);history.push_back(x);const auto y=engine->process(x,x,s);if(i>=engine->latency())error=std::max(error,std::abs(y.l-history[static_cast<size_t>(i-engine->latency())]));}
  require(error<1.e-10,"no-clipping null / declared latency");
  engine->prepare(rate);s.ceilingDb=-6.;s.inDb=12.;
  for(int i=0;i<18000;++i){const double x=.65*std::sin(2.*clip::pi*997.*i/rate)+.3*std::sin(2.*clip::pi*7103.*i/rate);const auto y=engine->process(x,-x,s);require(std::isfinite(y.l)&&std::abs(y.l)<=clip::dbGain(-6.)+1.e-9,"sample ceiling");require(std::abs(y.l+y.r)<1.e-10,"anti-phase preservation");}
  std::cout<<"rate "<<rate<<": null="<<error<<", ceiling and stereo OK\n";
 }
 std::mt19937 rng(73);std::uniform_real_distribution<double> random(-4.,4.);
 for(int mode=0;mode<3;++mode)for(int quality=0;quality<6;++quality){
  engine->prepare(48000.);clip::Settings s;s.mode=mode;s.quality=quality;s.ceilingDb=-3.;s.shape=23.;s.bass=48.;s.texture=-65.;s.punch=70.;s.focus=73.;s.asymmetry=32.;s.link=45.;
  double peak=0.;for(int i=0;i<9000;++i){const auto y=engine->process(random(rng),random(rng),s);require(std::isfinite(y.l)&&std::isfinite(y.r),"finite advanced output");peak=std::max({peak,std::abs(y.l),std::abs(y.r)});}
  require(peak<=clip::dbGain(-3.)+1.e-9,"ceiling with all processing");
  std::cout<<"mode "<<mode<<" quality "<<quality<<": ceiling="<<peak<<" OK\n";
 }
 engine->prepare(48000.);clip::Settings s;s.isp=false;s.dc=false;s.bypass=true;s.inDb=30.;s.outDb=12.;
 std::vector<double> dry;for(int i=0;i<10000;++i){const double x=random(rng)*.1;dry.push_back(x);const auto y=engine->process(x,x,s);if(i>4000)require(std::abs(y.l-dry[static_cast<size_t>(i-engine->latency())])<1.e-10,"bypass delayed null");}
 engine->prepare(48000.);s={};
 for(int i=0;i<4000;++i){const auto y=engine->process(i%13==0?std::numeric_limits<double>::quiet_NaN():0.,std::numeric_limits<double>::infinity(),s);require(std::isfinite(y.l)&&std::isfinite(y.r),"non-finite input sanitization");}
 // Exercise changes while the stream runs, including FIR quality crossfades.
 engine->prepare(48000.);s={};s.inDb=12.;s.ceilingDb=-6.;countAudioAllocations=true;
 for(int i=0;i<32000;++i){if(i%997==0){s.quality=(i/997)%6;s.mode=(i/997)%3;s.asymmetry=(i/997)%2?45.:-45.;s.mix=(i/997)%2?30.:100.;}
  const double x=.8*std::sin(2.*clip::pi*937.*i/48000.);const auto y=engine->process(x,.6*x,s);require(std::isfinite(y.l)&&std::abs(y.l)<=clip::dbGain(-6.)+1.e-9,"streaming automation ceiling");}
 countAudioAllocations=false;require(audioAllocations==0,"no audio-thread allocations");
 engine->prepare(48000.);s={};s.dc=false;s.isp=false;
 std::vector<double> reference(12000);for(int i=0;i<12000;++i){if(i%1013==0)s.quality=(i/1013)%6;const double x=.1*std::sin(2.*clip::pi*7301.*i/48000.);reference[static_cast<size_t>(i)]=x;const auto y=engine->process(x,x,s);if(i>=engine->latency())require(std::abs(y.l-reference[static_cast<size_t>(i-engine->latency())])<1.e-10,"quality-switch delayed null");}
 auto render=[&](const clip::Settings& settings){engine->prepare(48000.);std::vector<double> audio(36000);for(int i=0;i<18000;++i){const double burst=i%3600<200? .35*std::exp(-double(i%3600)/40.):0.;const double x=.32*std::sin(2.*clip::pi*73.*i/48000.)+.17*std::sin(2.*clip::pi*5973.*i/48000.)+burst;const double r=.14*std::sin(2.*clip::pi*127.*i/48000.);const auto y=engine->process(x,r,settings);audio[static_cast<size_t>(2*i)]=y.l;audio[static_cast<size_t>(2*i+1)]=y.r;}return audio;};
 clip::Settings base;base.mode=1;base.inDb=9.;base.ceilingDb=-6.;const auto baseline=render(base);
 struct Variation {const char* name;double clip::Settings::*parameter;double value;};
 const Variation variants[]{ {"shape",&clip::Settings::shape,60.},{"cleanliness",&clip::Settings::clean,0.},{"focus",&clip::Settings::focus,100.},{"punch",&clip::Settings::punch,100.},{"bass",&clip::Settings::bass,75.},{"texture",&clip::Settings::texture,-80.},{"stereo link",&clip::Settings::link,0.},{"asymmetry",&clip::Settings::asymmetry,60.},{"phase",&clip::Settings::phase,100.},{"release",&clip::Settings::releaseMs,200.},{"mix",&clip::Settings::mix,0.} };
 for(const auto& variant:variants){auto changed=base;changed.*(variant.parameter)=variant.value;const auto audio=render(changed);double difference=0.;for(size_t i=2000;i<audio.size();++i)difference=std::max(difference,std::abs(audio[i]-baseline[i]));require(difference>1.e-5,variant.name);std::cout<<variant.name<<": effect="<<difference<<'\n';}
 std::cout<<"Automation and allocation checks OK\n";
 for(int mode:{0,1})for(int quality:{2,5}){
  engine->prepare(48000.);s={};s.mode=mode;s.quality=quality;s.inDb=9.;
  const auto start=std::chrono::steady_clock::now();double accumulator=0.;
  for(int i=0;i<48000;++i){const double x=.7*std::sin(2.*clip::pi*97.*i/48000.)+.2*std::sin(2.*clip::pi*5973.*i/48000.);accumulator+=engine->process(x,x*.8,s).l;}
  const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
  std::cout<<(mode==0?"Clean ":"Perceptual ")<<(2<<quality)<<"x + ISP FIR: one stereo second in "<<seconds<<" seconds; checksum "<<accumulator<<'\n';
 }
 std::cout<<"PASS\n";
}

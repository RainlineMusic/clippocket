#include "../Source/ClipDSP.h"
#include <memory>
#include <iostream>
#include <random>
#include <chrono>
#include <cstdlib>
#include <vector>
#include <new>
static bool countAudioAllocations=false;static int audioAllocations=0;
void* operator new(std::size_t n){if(countAudioAllocations)++audioAllocations;if(auto* p=std::malloc(n))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}void operator delete(void* p,std::size_t) noexcept {std::free(p);}
static void require(bool b,const char* msg){if(!b){std::cerr<<"FAIL: "<<msg<<'\n';std::exit(1);}}
int main(){
 auto e=std::make_unique<clip::Engine>();
 for(double rate:{44100.,48000.,88200.,96000.,176400.,192000.})for(int quality=0;quality<6;++quality){
  e->prepare(rate);clip::Settings s;s.quality=quality;
  std::vector<double> history;double error=0.;
  for(int i=0;i<7000;++i){const double x=.1*std::sin(2.*clip::pi*997.*i/rate)+.05*std::sin(2.*clip::pi*7301.*i/rate);history.push_back(x);const auto y=e->process(x,x,s);if(i>=e->latency())error=std::max(error,std::abs(y.l-history[static_cast<size_t>(i-e->latency())]));}
  require(error<1.e-10,"below-threshold null, all rates and qualities");require(e->latency()==768,"constant declared latency");
 }
 for(double knee:{.0025,.015})for(int i=-10000;i<=10000;++i){const double x=i*.0005;require(std::abs(clip::curve(x,.5,knee))<=.5+1.e-12,"oversampled curve bound");}
 // Audio-rate output is reconstructed from the filtered correction. As in
 // oversampled clippers, it may overshoot: there is deliberately no ISP guard.
 e->prepare(48000.);clip::Settings s;s.inDb=12.;s.ceilingDb=-6.;
 double maxPeak=0.;for(int i=0;i<18000;++i){const double x=.65*std::sin(2.*clip::pi*997.*i/48000.)+.3*std::sin(2.*clip::pi*7103.*i/48000.);const auto y=e->process(x,-x,s);require(std::isfinite(y.l)&&std::abs(y.l+y.r)<1.e-10,"anti-phase preservation");maxPeak=std::max(maxPeak,std::abs(y.l));}
 require(maxPeak<.8,"reasonable reconstruction overshoot on test tones");
 // A loud right-hand event must not duck a quiet left channel.
 e->prepare(48000.);s={};std::vector<double> quiet;for(int i=0;i<10000;++i){const double x=.15*std::sin(2.*clip::pi*83.*i/48000.);quiet.push_back(x);const auto y=e->process(x,i%1000<20?5.:0.,s);if(i>=e->latency())require(std::abs(y.l-quiet[static_cast<size_t>(i-e->latency())])<1.e-10,"no cross-channel broadband ducking");}
 // No limiter release: after the FIR settles, quiet detail returns exactly.
 e->prepare(48000.);s={};for(int i=0;i<10000;++i){const double x=i<1000?3.:.1;const auto y=e->process(x,x,s);if(i>2000)require(std::abs(y.l-.1)<1.e-10,"no post-peak gain recovery");}
 e->prepare(48000.);s={};s.delta=true;for(int i=0;i<10000;++i){const auto y=e->process(.1,.1,s);require(std::abs(y.l)<1.e-10,"delta is zero below clipping");}
 e->prepare(48000.);s={};s.bypass=true;s.inDb=30.;s.outDb=12.;std::vector<double> dry;
 for(int i=0;i<10000;++i){const double x=.1*std::sin(2.*clip::pi*731.*i/48000.);dry.push_back(x);const auto y=e->process(x,x,s);if(i>4000)require(std::abs(y.l-dry[static_cast<size_t>(i-e->latency())])<1.e-10,"bypass delayed null");}
 e->prepare(48000.);s={};for(int i=0;i<3000;++i){const auto y=e->process(std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),s);require(std::isfinite(y.l)&&std::isfinite(y.r),"input sanitization");}
 // Kick Protect must discriminate a sub onset from a snare-body tone and a
 // steady bass. These are controlled stimuli, not a source-separation claim.
 auto burst=[&](double hz,double protect){e->prepare(48000.);clip::Settings t;t.bass=protect;std::vector<double> out(18000);double confidence=0.;for(int i=0;i<18000;++i){const double time=(i-3000)/48000.;const double x=time>=0.?2.5*std::sin(2.*clip::pi*hz*time)*std::exp(-time/.035):0.;const auto y=e->process(x,x,t);out[static_cast<size_t>(i)]=y.l;confidence=std::max(confidence,y.kickConfidence);}return std::make_pair(out,confidence);};
 const auto kick0=burst(55.,0.),kick100=burst(55.,100.),snare0=burst(220.,0.),snare100=burst(220.,100.);
 double kickDelta=0.,snareDelta=0.;for(size_t i=0;i<kick0.first.size();++i){kickDelta+=std::pow(kick0.first[i]-kick100.first[i],2);snareDelta+=std::pow(snare0.first[i]-snare100.first[i],2);}
 double originalProjection=0.,protectedProjection=0.;
 for(size_t i=768;i<kick0.first.size();++i){const double time=(static_cast<double>(i)-768.-3000.)/48000.;const double x=time>=0.?2.5*std::sin(2.*clip::pi*55.*time)*std::exp(-time/.035):0.;originalProjection+=kick0.first[i]*x;protectedProjection+=kick100.first[i]*x;}
 require(protectedProjection>originalProjection*1.005,"phase-aligned protection restores kick fundamental");
 require(kick100.second>.2,"kick onset detected");require(snare100.second<.05,"snare-body rejection");require(kickDelta>1.e-5,"kick protection audible effect");require(snareDelta<kickDelta*.01,"protection is selective for sub onsets");
 clip::Perception detector;detector.prepare(48000.);double sustained=0.;for(int i=0;i<96000;++i){detector.process(1.5*std::sin(2.*clip::pi*55.*i/48000.));if(i>48000)sustained=std::max(sustained,detector.kick());}require(sustained<.05,"sustained bass is not repeatedly labelled kick");
 std::cout<<"Kick confidence "<<kick100.second<<", snare "<<snare100.second<<", steady bass "<<sustained<<"; squared changes "<<kickDelta<<" / "<<snareDelta<<'\n';
 e->prepare(48000.);s={};s.inDb=12.;countAudioAllocations=true;
 for(int i=0;i<30000;++i){if(i%997==0){s.quality=(i/997)%6;s.bass=(i/997)%2?100.:0.;s.delta=(i/997)%3==0;}const double x=.8*std::sin(2.*clip::pi*937.*i/48000.);const auto y=e->process(x,.6*x,s);require(std::isfinite(y.l)&&std::isfinite(y.r),"streaming automation finite");}
 countAudioAllocations=false;require(audioAllocations==0,"no audio-thread allocations");
 e->prepare(48000.);s={};std::vector<double> reference(12000);for(int i=0;i<12000;++i){if(i%1013==0)s.quality=(i/1013)%6;const double x=.1*std::sin(2.*clip::pi*7301.*i/48000.);reference[static_cast<size_t>(i)]=x;const auto y=e->process(x,x,s);if(i>=e->latency())require(std::abs(y.l-reference[static_cast<size_t>(i-e->latency())])<1.e-10,"quality automation null");}
 std::cout<<"PASS: 6 sample rates, 6 qualities, quiet-signal null, no recovery/ducking, kick discrimination, delta, bypass and allocations\n";
}

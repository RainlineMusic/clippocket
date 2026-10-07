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
  require(error<1.e-10,"below-threshold null, all rates and qualities");require(e->latency()==512+int(std::ceil(rate*.030))+128,"constant declared latency");
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
 e->prepare(48000.);s={};for(int i=0;i<10000;++i){const double x=i<1000?3.:.1;const auto y=e->process(x,x,s);if(i>4000)require(std::abs(y.l-.1)<1.e-10,"no post-peak gain recovery");}
 e->prepare(48000.);s={};s.bypass=true;s.inDb=30.;s.outDb=12.;std::vector<double> dry;
 for(int i=0;i<10000;++i){const double x=.1*std::sin(2.*clip::pi*731.*i/48000.);dry.push_back(x);const auto y=e->process(x,x,s);if(i>4000)require(std::abs(y.l-dry[static_cast<size_t>(i-e->latency())])<1.e-10,"bypass delayed null");}
 e->prepare(48000.);s={};for(int i=0;i<3000;++i){const auto y=e->process(std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),s);require(std::isfinite(y.l)&&std::isfinite(y.r),"input sanitization");}
 // Kick Protect must discriminate a sub onset from a snare-body tone and a
 // steady bass. These are controlled stimuli, not a source-separation claim.
 auto burst=[&](double hz,double protect){e->prepare(48000.);clip::Settings t;t.bass=protect;std::vector<double> out(18000);double confidence=0.;for(int i=0;i<18000;++i){const double time=(i-3000)/48000.;const double x=time>=0.?2.5*std::sin(2.*clip::pi*hz*time)*std::exp(-time/.035):0.;const auto y=e->process(x,x,t);out[static_cast<size_t>(i)]=y.l;confidence=std::max(confidence,y.kickConfidence);}return std::make_pair(out,confidence);};
 const auto kick0=burst(55.,0.),kick100=burst(55.,100.),snare0=burst(220.,0.),snare100=burst(220.,100.);
 double kickDelta=0.,snareDelta=0.;for(size_t i=0;i<kick0.first.size();++i){kickDelta+=std::pow(kick0.first[i]-kick100.first[i],2);snareDelta+=std::pow(snare0.first[i]-snare100.first[i],2);}
 double originalProjection=0.,protectedProjection=0.;
 for(size_t i=static_cast<size_t>(e->latency());i<kick0.first.size();++i){const double time=(static_cast<double>(i)-e->latency()-3000.)/48000.;const double x=time>=0.?2.5*std::sin(2.*clip::pi*55.*time)*std::exp(-time/.035):0.;originalProjection+=kick0.first[i]*x;protectedProjection+=kick100.first[i]*x;}
 require(protectedProjection>0.,"protected kick retains polarity");
 require(kick100.second>.2,"kick onset detected");require(snare100.second<.05,"snare-body rejection");require(kickDelta>1.e-5,"kick protection audible effect");require(snareDelta<kickDelta*.01,"protection is selective for sub onsets");
 clip::Perception detector;detector.prepare(48000.);double sustained=0.;for(int i=0;i<96000;++i){detector.process(1.5*std::sin(2.*clip::pi*55.*i/48000.));if(i>48000)sustained=std::max(sustained,detector.kick());}require(sustained<.05,"sustained bass is not repeatedly labelled kick");
 std::cout<<"Kick confidence "<<kick100.second<<", snare "<<snare100.second<<", steady bass "<<sustained<<"; squared changes "<<kickDelta<<" / "<<snareDelta<<'\n';
 e->prepare(48000.);s={};s.inDb=12.;countAudioAllocations=true;
 for(int i=0;i<30000;++i){if(i%997==0){s.quality=(i/997)%6;s.bass=(i/997)%2?100.:0.;s.mode=(i/997)%3;}const double x=.8*std::sin(2.*clip::pi*937.*i/48000.);const auto y=e->process(x,.6*x,s);require(std::isfinite(y.l)&&std::isfinite(y.r),"streaming automation finite");}
 countAudioAllocations=false;require(audioAllocations==0,"no audio-thread allocations");
 e->prepare(48000.);s={};std::vector<double> reference(12000);for(int i=0;i<12000;++i){if(i%1013==0)s.quality=(i/1013)%6;const double x=.1*std::sin(2.*clip::pi*7301.*i/48000.);reference[static_cast<size_t>(i)]=x;const auto y=e->process(x,x,s);if(i>=e->latency())require(std::abs(y.l-reference[static_cast<size_t>(i-e->latency())])<1.e-10,"quality automation null");}
 // Stationary low tones: compare harmonic energy against fundamental.
 auto thd=[&](double hz,double protect){e->prepare(48000.);clip::Settings t;t.bass=protect;t.quality=3;double fundamental=0.,harmonics=0.;std::array<double,10> cs{},sn{};for(int i=0;i<96000;++i){const auto y=e->process(2.5*std::sin(2.*clip::pi*hz*i/48000.),0.,t);if(i>=48000)for(int h=1;h<=10;++h){const double ph=2.*clip::pi*hz*h*(i-e->latency())/48000.;cs[h-1]+=y.l*std::cos(ph);sn[h-1]+=y.l*std::sin(ph);}}for(int h=0;h<10;++h){const double v=cs[h]*cs[h]+sn[h]*sn[h];if(h==0)fundamental=v;else harmonics+=v;}return std::sqrt(harmonics/fundamental);};
 for(double hz:{40.,55.,80.,100.}){const auto bare=thd(hz,0.),guard=thd(hz,100.);std::cout<<hz<<" Hz THD "<<bare<<" -> "<<guard<<'\n';require(guard<bare*.4,"Low Protect reduces LF harmonics");}
 for(int mode=0;mode<3;++mode){e->prepare(48000.);clip::Settings t;t.mode=mode;for(int i=0;i<12000;++i){const auto y=e->process(3.*std::sin(i*.071),2.*std::sin(i*.119),t);require(std::isfinite(y.l)&&std::isfinite(y.r)&&std::abs(y.l)<8.,"all modes finite and bounded on two-tone stimulus");}}
 // Quiet signal remains a delayed null with protection enabled.
 e->prepare(48000.);s={};s.bass=100.;std::vector<double> lowQuiet(12000);for(int i=0;i<12000;++i){const double x=.5*std::sin(i*2.*clip::pi*55./48000.);lowQuiet[i]=x;const auto y=e->process(x,x,s);if(i>=e->latency())require(std::abs(y.l-lowQuiet[i-e->latency()])<1.e-10,"Low Protect does not change quiet bass");}
 auto im=[&](int mode){e->prepare(48000.);clip::Settings t;t.mode=mode;double re=0.,imag=0.;for(int i=0;i<96000;++i){const double x=1.2*std::sin(i*2.*clip::pi*11000./48000.)+1.2*std::sin(i*2.*clip::pi*21000./48000.);const auto y=e->process(x,x,t);if(i>=48000){const double phase=2.*clip::pi*1000.*i/48000.;re+=y.l*std::cos(phase);imag+=y.l*std::sin(phase);}}return std::hypot(re,imag);};
 const auto imClean=im(0),imCancel=im(1);std::cout<<"1 kHz IM Clean/Cancel "<<imClean<<" / "<<imCancel<<'\n';require(imCancel<imClean,"Punchy suppresses the measured low difference product");
 // Compare relative IM product, normalised by retained carrier, not raw level.
 for(auto tones:{std::array<double,3>{11000,21000,1000},{7000,13000,1000},{5000,9000,1000},{60,7000,6880},{55,1000,890}}){std::array<double,2> ratios{};for(int enabled=0;enabled<2;++enabled){e->prepare(48000.);clip::Settings t;t.mode=1;t.imdClean=enabled!=0;std::array<double,4> projection{};for(int i=0;i<72000;++i){auto y=e->process(1.2*std::sin(2*clip::pi*tones[0]*i/48000)+1.2*std::sin(2*clip::pi*tones[1]*i/48000),0.,t);if(i>=24000)for(int k=0;k<2;++k){const double ph=2*clip::pi*tones[k+1]*i/48000;projection[2*k]+=y.l*std::cos(ph);projection[2*k+1]+=y.l*std::sin(ph);}}ratios[enabled]=std::hypot(projection[2],projection[3])/std::hypot(projection[0],projection[1]);}const double change=clip::gainDb(ratios[1]/ratios[0]);std::cout<<"IMD A/B "<<tones[0]<<"+"<<tones[1]<<" change "<<change<<" dB\n";require(change<(tones[0]>1000?-3.:.1),"IMD Clean benefits high-tone probes, bounded bass regression");}
 clip::LowShape shape;shape.reset();for(int i=0;i<6000;++i){const double x=2.5*std::sin(2*clip::pi*55*i/48000);const double y=shape.process(x,1440,1.,.3,x);if(i>2500){const double reference=2.5*std::sin(2*clip::pi*55*(i-1440)/48000);require(std::abs(y-reference*.36)<1.e-5,"completed half-wave preserves sample ratios");}}
 // Bound source-rate samples, including reconstruction, Low Protect, positive
 // Output Gain, soft knee and mode/quality automation. No true-peak assertion.
 for(double rate:{44100.,48000.,192000.})for(int mode=0;mode<3;++mode){e->prepare(rate);clip::Settings t;t.mode=mode;t.inDb=36.;t.outDb=12.;t.bass=100.;double peak=0.;for(int i=0;i<9000;++i){t.quality=(i/1300)%6;t.knee=(i/2200)%2?100.:0.;const double x=3.*std::sin(i*.271)+2.*std::sin(i*.713);const auto y=e->process(x,-x*.8,t);peak=std::max({peak,std::abs(y.l),std::abs(y.r)});require(std::isfinite(y.l)&&peak<=clip::dbGain(12.)+1.e-12,"sample boundary before final Output Gain");}}
 e->prepare(48000.);s={};s.ceilingDb=-6.;s.outDb=-3.;const double cap=clip::dbGain(-9.);for(int i=0;i<9000;++i){const auto y=e->process(4.*std::sin(i*.21),4.,s);require(std::max(std::abs(y.l),std::abs(y.r))<=cap+1.e-12,"negative ceiling/output sample bound");}
 e->prepare(48000.);s={};s.outDb=1.;double positive=0.;for(int i=0;i<10000;++i){auto y=e->process(4.,4.,s);positive=std::max(positive,y.l);}require(std::abs(positive-clip::dbGain(1.))<1.e-12,"Output +1 dB produces +1 dBFS");
 require(std::abs(clip::curve(1.1,1.,0.)-1.)<1.e-12,"zero knee is hard clipping");require(clip::curve(.8,1.,.95)<.8,"soft knee begins below threshold");
 std::cout<<"PASS: 6 sample rates, 6 qualities, quiet-signal null, no recovery/ducking, kick discrimination, sample bound, knee, bypass and allocations\n";
}

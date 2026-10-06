#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>

// Independent C++17 DSP; no JUCE, allocations, locks or host calls in process().
namespace clip {
constexpr double pi=3.14159265358979323846;
inline double finite(double v,double fallback=0.) noexcept {return std::isfinite(v)?v:fallback;}
inline double dbGain(double db) noexcept {return std::pow(10.,std::clamp(finite(db),-120.,60.)/20.);}
inline double gainDb(double x) noexcept {return 20.*std::log10(std::max(1.e-12,std::abs(x)));}
inline double pole(double hz,double sr) noexcept {return std::exp(-2.*pi*hz/sr);}
inline double bessel0(double x) noexcept {
 double sum=1.,term=1.;for(int k=1;k<40;++k){term*=x*x/(4.*k*k);sum+=term;if(term<sum*1.e-15)break;}return sum;
}
inline double sinc(double x) noexcept {return std::abs(x)<1.e-12?1.:std::sin(pi*x)/(pi*x);}
struct Settings {
 double inDb=0.,ceilingDb=0.,outDb=0.,shape=0.,clean=100.,focus=50.,punch=0.;
 double bass=0.,texture=0.,link=100.,asymmetry=0.,phase=0.,releaseMs=30.,mix=100.;
 int quality=2,mode=0;bool dc=true,isp=true,autoGain=false,delta=false,bypass=false;
};
struct Result {double l=0.,r=0.,inputPeak=0.,reductionDb=0.,guardDb=0.,clipActivity=0.;};
struct OnePole {double s=0.; double low(double x,double a) noexcept {s=(1.-a)*x+a*s;return s;} void reset() noexcept {s=0.;}};
class Delay {
public:
 static constexpr int capacity=4096;
 double push(double x,int d) noexcept {data[static_cast<size_t>(pos)]=x;const double out=data[static_cast<size_t>((pos-d+capacity)%capacity)];pos=(pos+1)%capacity;return out;}
 void reset() noexcept {data.fill(0.);pos=0;}
private:std::array<double,capacity> data{};int pos=0;
};
class FFT512 {
public:
 static constexpr int size=512;
 FFT512(){for(int k=0;k<size/2;++k)twiddle[static_cast<size_t>(k)]=std::polar(1.,-2.*pi*k/size);}
 void run(std::array<std::complex<double>,size>& v,bool inverse=false) const noexcept {
  for(int i=1,j=0;i<size;++i){int bit=size>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(v[static_cast<size_t>(i)],v[static_cast<size_t>(j)]);}
  for(int len=2;len<=size;len*=2)for(int base=0;base<size;base+=len)for(int j=0;j<len/2;++j){
   auto w=twiddle[static_cast<size_t>(j*size/len)];if(inverse)w=std::conj(w);
   const auto a=v[static_cast<size_t>(base+j)],b=v[static_cast<size_t>(base+j+len/2)]*w;
   v[static_cast<size_t>(base+j)]=a+b;v[static_cast<size_t>(base+j+len/2)]=a-b;
  }
  if(inverse)for(auto& x:v)x/=size;
 }
private:std::array<std::complex<double>,size/2> twiddle{};
};
inline double curve(double x,double ceiling,double shape,double asym) noexcept {
 const double sign=x<0.?-1.:1.;
 const double t=ceiling*(1.-.30*std::max(0.,sign*asym));
 const double a=std::abs(x),k=std::clamp(shape*.005,0.,.5);
 if(k<1.e-6)return sign*std::min(a,t);
 const double lo=t*(1.-k),hi=t*(1.+k);
 if(a<=lo)return x;
 if(a>=hi)return sign*t;
 const double u=(a-lo)/(hi-lo);return sign*(lo+(hi-lo)*(u-.5*u*u));
}
// US6337999 differential topology, with sparse 65-tap halfband FIR stages.
// The cascade has 64*(1-1/M) base-rate samples of round-trip group delay;
// correction-only padding makes every quality exactly 64 samples.
class Differential {
public:
 static constexpr int delay=64;
 void prepare() noexcept {
  double sum=0.;const double normal=bessel0(10.5);
  for(int i=0;i<32;++i){const double at=2.*i+1.-32.;const double z=at/32.;
   odd[static_cast<size_t>(i)]=.5*sinc(.5*at)*bessel0(10.5*std::sqrt(std::max(0.,1.-z*z)))/normal;sum+=odd[static_cast<size_t>(i)];}
  for(auto& h:odd)h*=.5/sum;
  reset();
 }
 void reset() noexcept {for(auto& x:up)x.reset();for(auto& x:down)x.reset();dry.reset();padding.reset();quality=-1;lastReduction=0.;ratio=1.;}
 void begin(double x,int q) noexcept {
  q=std::clamp(q,0,5);if(q!=quality){for(auto& h:up)h.reset();for(auto& h:down)h.reset();padding.reset();quality=q;}
  high[0]=x;ratio=1.;int count=1;
  for(int stage=0;stage<=quality;++stage){auto& state=up[static_cast<size_t>(stage)];
   for(int i=0;i<count;++i){state.push(high[static_cast<size_t>(i)]);const auto* history=state.data.data()+state.pos+65;
    scratch[static_cast<size_t>(2*i)]=history[-16];double value=0.;for(int j=0;j<32;++j)value+=odd[static_cast<size_t>(j)]*history[-j];scratch[static_cast<size_t>(2*i+1)]=2.*value;state.advance();}
   count*=2;std::copy_n(scratch.begin(),count,high.begin());
  }
 }
 int factor() const noexcept {return 2<<quality;}
 double interpolate(int phase) const noexcept {return high[static_cast<size_t>(phase)];}
 void correct(double input,double shaped,int phase,double threshold) noexcept {
  errors[static_cast<size_t>(phase)]=input-shaped;
  if(std::abs(input)>threshold*.25)ratio=std::max(ratio,std::abs(input)/std::max(1.e-12,std::abs(shaped)));
 }
 double end(double x) noexcept {
  int count=factor();
  for(int stage=quality;stage>=0;--stage){auto& state=down[static_cast<size_t>(stage)];
   for(int i=0;i<count;i+=2){state.push(errors[static_cast<size_t>(i)]);const auto* history=state.data.data()+state.pos+65;
    double value=.5*history[-32];for(int j=0;j<32;++j)value+=odd[static_cast<size_t>(j)]*history[-(2*j+1)];scratch[static_cast<size_t>(i/2)]=value;state.advance();state.push(errors[static_cast<size_t>(i+1)]);state.advance();}
   count/=2;std::copy_n(scratch.begin(),count,errors.begin());
  }
  const double removed=padding.push(errors[0],64/factor());delayed=dry.push(x,delay);lastReduction=gainDb(ratio);return delayed-removed;
 }
 double original() const noexcept {return delayed;}
 double reduction() const noexcept {return lastReduction;}
private:
 struct State {
  std::array<double,130> data{};int pos=0;
  void reset() noexcept {data.fill(0.);pos=0;}
  void push(double x) noexcept {data[static_cast<size_t>(pos)]=data[static_cast<size_t>(pos+65)]=x;}
  void advance() noexcept {if(++pos==65)pos=0;}
 };
 std::array<State,6> up{},down{};std::array<double,32> odd{};std::array<double,64> high{},errors{},scratch{};
 Delay dry,padding;int quality=-1;double delayed=0.,lastReduction=0.,ratio=1.;
};
// Spectral analysis controls the oversampled knee; it never clips a host-rate
// FFT frame. This avoids the folded harmonics of host-rate waveform projection.
class MaskingDetector {
public:
 static constexpr int size=512,hop=128,latency=size;
 void prepare() noexcept {for(int i=0;i<size;++i)window[static_cast<size_t>(i)]=.5-.5*std::cos(2.*pi*i/size);reset();}
 void reset() noexcept {input.fill(0.);previous.fill(0.);clock=0;tonality=treble=attack=0.;}
 void process(double x,double sr,bool enabled) noexcept {
  input[static_cast<size_t>(clock%size)]=x;++clock;
  if(!enabled||clock%hop!=0)return;
  double peak=0.,rms=0.;
  for(int i=0;i<size;++i){const auto j=static_cast<size_t>(i);const double v=input[static_cast<size_t>((clock+static_cast<std::uint64_t>(i))%size)];peak=std::max(peak,std::abs(v));rms+=v*v;spectrum[j]={v*window[j],0.};}
  fft.run(spectrum);double energy=1.e-20,strongest=0.,high=0.,flux=0.;
  for(int k=1;k<size/2;++k){const auto j=static_cast<size_t>(k);const double power=std::norm(spectrum[j]);energy+=power;strongest=std::max(strongest,power);if(double(k)*sr/size>6000.)high+=power;flux+=std::max(0.,power-previous[j]);previous[j]=power;}
  tonality=std::clamp(strongest/energy*1.5,0.,1.);treble=std::clamp(high/energy,0.,1.);
  const double crest=peak/std::max(1.e-12,std::sqrt(rms/size));attack=std::clamp(.55*flux/energy+.15*std::max(0.,crest-2.),0.,1.);
 }
 double knee(const Settings& s) const noexcept {
  const double protection=(.35+.45*tonality+.20*treble)*(1.-s.focus*.006);
  const double soften=s.clean*.65*protection*(1.-s.punch*.009*attack);
  return std::clamp(s.shape+soften,0.,100.);
 }
private:
 FFT512 fft;std::array<double,size> input{},window{},previous{};std::array<std::complex<double>,size> spectrum{};
 std::uint64_t clock=0;double tonality=0.,treble=0.,attack=0.;
};
// Windowed-sinc sub-sample peak predictor. Three FIR phases plus the original
// samples estimate 4x reconstructed peaks; this is not a certified TP meter.
class PeakInterpolator {
public:
 void prepare() noexcept {
  const double normal=bessel0(8.6);
  for(int ph=0;ph<3;++ph){double sum=0.;for(int j=0;j<33;++j){const double z=(j-16.)/16.;const double h=sinc(j-16.+(ph+1)*.25)*bessel0(8.6*std::sqrt(std::max(0.,1.-z*z)))/normal;coeff[static_cast<size_t>(ph)][static_cast<size_t>(j)]=h;sum+=h;}for(auto& h:coeff[static_cast<size_t>(ph)])h/=sum;}
  reset();
 }
 void reset() noexcept {history.fill(0.);pos=0;}
 double process(double x,bool enabled) noexcept {
  history[static_cast<size_t>(pos)]=history[static_cast<size_t>(pos+33)]=x;double peak=std::abs(x);
  if(enabled){const auto* h=history.data()+pos+33;for(const auto& phase:coeff){double value=0.;for(int j=0;j<33;++j)value+=phase[static_cast<size_t>(j)]*h[-j];peak=std::max(peak,std::abs(value));}}
  if(++pos==33)pos=0;
  return peak;
 }
private:std::array<std::array<double,33>,3> coeff{};std::array<double,66> history{};int pos=0;
};
// Fixed-latency safety limiter. The sample ceiling is bounded independently
// of the FIR predictor; no host-rate hard clip is added after the clipper.
class Guard {
public:
 void prepare(double sr) noexcept {sampleRate=sr;lookahead=std::clamp(int(std::ceil(sr*.001)),16,256);for(auto& p:predictor)p.prepare();reset();}
 void reset() noexcept {values.fill(0.);left.reset();right.reset();pos=0;head=tail=0;clock=0;gain=1.;for(auto& p:predictor)p.reset();lastGain=1.;}
 int latency() const noexcept {return lookahead;}
 std::array<double,2> process(double l,double r,double ceiling,double releaseMs,bool isp) noexcept {
  const double peak=std::max(predictor[0].process(l,isp),predictor[1].process(r,isp));
  while(head!=tail&&values[static_cast<size_t>((tail+511)%512)]<=peak)tail=(tail+511)%512;
  values[static_cast<size_t>(tail)]=peak;ages[static_cast<size_t>(tail)]=clock;tail=(tail+1)%512;
  while(head!=tail&&ages[static_cast<size_t>(head)]+static_cast<std::uint64_t>(lookahead)<clock)head=(head+1)%512;
  const double maximum=head==tail?0.:values[static_cast<size_t>(head)];++clock;
  const double safe=ceiling*(isp?dbGain(-.12):1.);const double wanted=std::min(1.,safe/std::max(1.e-12,maximum));
  const double a=std::exp(-1./(std::clamp(releaseMs,1.,250.)*.001*sampleRate));
  gain=wanted<gain?wanted:wanted+a*(gain-wanted);
  const double dl=left.push(l,lookahead),dr=right.push(r,lookahead);pos=(pos+1)%static_cast<int>(values.size());lastGain=gain;
  // Invariant: the delayed samples are in the peak window, so no post-clipper
  // hard clamp is needed and a final un-antialiased discontinuity is avoided.
  return {dl*gain,dr*gain};
 }
 double reduction() const noexcept {return -gainDb(lastGain);}
private:
 std::array<double,512> values{};std::array<std::uint64_t,512> ages{};std::uint64_t clock=0;int head=0,tail=0;std::array<PeakInterpolator,2> predictor;Delay left,right;
 double sampleRate=48000.,gain=1.,lastGain=1.;int lookahead=48,pos=0;
};
class Engine {
public:
 void prepare(double sr) noexcept {
  sampleRate=std::clamp(finite(sr,48000.),8000.,384000.);
  for(auto& bank:differential)for(auto& d:bank)d.prepare();
  for(auto& p:masking)p.prepare();
  guard.prepare(sampleRate);
  bassPole=pole(180.,sampleRate);texturePole=pole(3500.,sampleRate);dcPole=pole(5.,sampleRate);
  smoothPole=std::exp(-1./(.010*sampleRate));phasePole=(std::tan(pi*140./sampleRate)-1.)/(std::tan(pi*140./sampleRate)+1.);reset();
 }
 void reset() noexcept {
  for(auto& bank:differential)for(auto& d:bank)d.reset();
  for(auto& p:masking)p.reset();
  for(auto& d:analysisDelay)d.reset();
  guard.reset();
  for(auto& d:dryDelay)d.reset();
  for(auto& d:referenceDelay)d.reset();
  for(auto& p:bassFilter)p.reset();
  for(auto& p:textureFilter)p.reset();
  for(auto& p:dcFilter)p.reset();
  phaseX={};phaseY={};controls=Settings{};primed=false;wet=1.;adaptiveShape=0.;autoBlend=deltaBlend=0.;activeBank=0;activeQuality=2;nextQuality=2;qualityFade=0;
 }
 int latency() const noexcept {return Differential::delay+MaskingDetector::latency+guard.latency();}
 Result process(double l,double r,Settings target,int channels=2) noexcept {
  sanitize(target);if(target.mode==2)target.shape=50.+target.shape*.5;if(!primed){controls=target;wet=target.bypass?0.:1.;adaptiveShape=target.shape;autoBlend=target.autoGain?1.:0.;deltaBlend=target.delta?1.:0.;activeQuality=target.quality;primed=true;}
  smooth(target);const double rawL=std::clamp(finite(l),-64.,64.),rawR=channels==1?rawL:std::clamp(finite(r),-64.,64.);
  const double dryL=dryDelay[0].push(rawL,latency()),dryR=dryDelay[1].push(rawR,latency());
  const double inGain=dbGain(controls.inDb),ceiling=dbGain(controls.ceilingDb);
  std::array<double,2> input{rawL*inGain,rawR*inGain},processed{},original{};
  for(int c=0;c<2;++c){const auto j=static_cast<size_t>(c);masking[j].process(input[j],sampleRate,controls.mode==1);input[j]=analysisDelay[j].push(input[j],MaskingDetector::latency);}
  const double desiredShape=controls.mode==1?std::max(masking[0].knee(controls),masking[1].knee(controls)):controls.shape;
  adaptiveShape=desiredShape+(adaptiveShape-desiredShape)*smoothPole;

  for(int c=0;c<2;++c){const auto j=static_cast<size_t>(c);const double phaseOutput=phasePole*input[j]+phaseX[j]-phasePole*phaseY[j];phaseX[j]=input[j];phaseY[j]=phaseOutput;
   input[j]+=controls.phase*.01*(phaseOutput-input[j]);
   }
  if(qualityFade==0&&controls.quality!=activeQuality){nextQuality=controls.quality;for(auto& d:differential[static_cast<size_t>(1-activeBank)])d.reset();qualityFade=1;}
  double clippingReduction=0.;
  auto runBank=[&](int bank,int quality,std::array<double,2>& destination){
   auto& pair=differential[static_cast<size_t>(bank)];
   for(int c=0;c<2;++c)pair[static_cast<size_t>(c)].begin(input[static_cast<size_t>(c)],quality);
   for(int ph=0;ph<pair[0].factor();++ph){
    const double a=pair[0].interpolate(ph),b=pair[1].interpolate(ph);
    const double ca=curve(a,ceiling,adaptiveShape,controls.asymmetry*.01),cb=curve(b,ceiling,adaptiveShape,controls.asymmetry*.01);
    const double common=std::min(std::abs(a)>1.e-12?std::abs(ca/a):1.,std::abs(b)>1.e-12?std::abs(cb/b):1.);
    pair[0].correct(a,ca+controls.link*.01*(a*common-ca),ph,ceiling);
    pair[1].correct(b,cb+controls.link*.01*(b*common-cb),ph,ceiling);
   }
   for(int c=0;c<2;++c){const auto j=static_cast<size_t>(c);destination[j]=pair[j].end(input[j]);clippingReduction=std::max(clippingReduction,pair[j].reduction());}
  };
  runBank(activeBank,activeQuality,processed);
  for(int c=0;c<2;++c)original[static_cast<size_t>(c)]=differential[static_cast<size_t>(activeBank)][static_cast<size_t>(c)].original();
  if(qualityFade>0){std::array<double,2> next{};runBank(1-activeBank,nextQuality,next);
   // Warm the new FIR for 128 samples, then crossfade for 512 samples.
   // Both banks share the same 64-sample delay, even when factors differ.
   const double blend=std::clamp((qualityFade-128.)/512.,0.,1.);
   for(int c=0;c<2;++c){const auto j=static_cast<size_t>(c);processed[j]+=blend*(next[j]-processed[j]);}
   if(++qualityFade>640){activeBank=1-activeBank;activeQuality=nextQuality;qualityFade=0;}
  }
  double activity=0.;
  for(int c=0;c<2;++c){const auto j=static_cast<size_t>(c);double residual=original[j]-processed[j];activity=std::max(activity,std::abs(residual)/std::max(1.e-12,ceiling));
   const double low=bassFilter[j].low(residual,bassPole);residual-=controls.bass*.01*low;
   const double dark=textureFilter[j].low(residual,texturePole);residual+=controls.texture*.005*(residual-dark);
   const double dc=dcFilter[j].low(residual,dcPole);if(controls.dc)residual-=dc;
   processed[j]=original[j]-residual;
  }
  for(int c=0;c<2;++c){const auto j=static_cast<size_t>(c);processed[j]=original[j]+controls.mix*.01*(processed[j]-original[j]);original[j]=referenceDelay[j].push(original[j],guard.latency());}
  const auto limited=guard.process(processed[0],processed[1],ceiling,controls.releaseMs,controls.isp);
  autoBlend=(controls.autoGain?1.:0.)+(autoBlend-(controls.autoGain?1.:0.))*smoothPole;
  deltaBlend=(controls.delta?1.:0.)+(deltaBlend-(controls.delta?1.:0.))*smoothPole;
  const double autoComp=dbGain(-std::max(0.,controls.inDb)*autoBlend);const double outGain=dbGain(controls.outDb)*autoComp;
  const double wetTarget=target.bypass?0.:1.;wet=wetTarget+(wet-wetTarget)*std::exp(-1./(.005*sampleRate));if(std::abs(wet-wetTarget)<1.e-12)wet=wetTarget;
  Result result;const double wl=limited[0]+deltaBlend*(original[0]-2.*limited[0]),wr=limited[1]+deltaBlend*(original[1]-2.*limited[1]);
  result.l=dryL+wet*(wl*outGain-dryL);result.r=dryR+wet*(wr*outGain-dryR);
  result.inputPeak=std::max(std::abs(rawL*inGain),std::abs(rawR*inGain));
  result.guardDb=guard.reduction();result.reductionDb=target.bypass?0.:clippingReduction+result.guardDb;
  result.clipActivity=activity;return result;
 }
private:
 static void sanitize(Settings& s) noexcept {
  auto clamp=[](double& x,double lo,double hi,double fallback){x=std::clamp(finite(x,fallback),lo,hi);};
  clamp(s.inDb,-24.,36.,0.);clamp(s.ceilingDb,-30.,0.,0.);clamp(s.outDb,-24.,12.,0.);
  for(auto* x:{&s.shape,&s.clean,&s.focus,&s.punch,&s.bass,&s.link,&s.phase,&s.mix})clamp(*x,0.,100.,0.);
  clamp(s.texture,-100.,100.,0.);clamp(s.asymmetry,-100.,100.,0.);clamp(s.releaseMs,1.,250.,30.);s.mode=std::clamp(s.mode,0,2);s.quality=std::clamp(s.quality,0,5);
 }
 void smooth(const Settings& t) noexcept {
  auto move=[this](double& x,double y){x=y+(x-y)*smoothPole;};
  move(controls.inDb,t.inDb);move(controls.ceilingDb,t.ceilingDb);move(controls.outDb,t.outDb);move(controls.shape,t.shape);move(controls.clean,t.clean);move(controls.focus,t.focus);move(controls.punch,t.punch);move(controls.bass,t.bass);move(controls.texture,t.texture);move(controls.link,t.link);move(controls.asymmetry,t.asymmetry);move(controls.phase,t.phase);move(controls.releaseMs,t.releaseMs);move(controls.mix,t.mix);
  controls.quality=t.quality;controls.mode=t.mode;controls.dc=t.dc;controls.isp=t.isp;controls.autoGain=t.autoGain;controls.delta=t.delta;
 }
 std::array<std::array<Differential,2>,2> differential;std::array<MaskingDetector,2> masking;Guard guard;
 std::array<Delay,2> dryDelay,referenceDelay,analysisDelay;std::array<OnePole,2> bassFilter,textureFilter,dcFilter;
 std::array<double,2> phaseX{},phaseY{};Settings controls;double sampleRate=48000.,bassPole=0.,texturePole=0.,dcPole=0.,smoothPole=0.,phasePole=0.,wet=1.,adaptiveShape=0.,autoBlend=0.,deltaBlend=0.;bool primed=false;int activeBank=0,activeQuality=2,nextQuality=2,qualityFade=0;
};
}

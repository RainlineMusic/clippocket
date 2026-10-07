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
 double inDb=0.,ceilingDb=0.,outDb=0.,bass=0.,knee=0.;
 int quality=3,mode=0;bool bypass=false,imdClean=true;
};
struct Result {double l=0.,r=0.,inputPeak=0.,reductionDb=0.,clipActivity=0.,kickConfidence=0.;};
struct OnePole {double s=0.; double low(double x,double a) noexcept {s=(1.-a)*x+a*s;return s;} void reset() noexcept {s=0.;}};
template<int capacity> class FixedDelay {
public:
 double push(double x,int d) noexcept {data[static_cast<size_t>(pos)]=x;const double out=data[static_cast<size_t>((pos-d+capacity)%capacity)];pos=(pos+1)%capacity;return out;}
 void reset() noexcept {data.fill(0.);pos=0;}
private:std::array<double,capacity> data{};int pos=0;
};
using Delay=FixedDelay<32768>;
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
// A narrow, C1-continuous knee. Everything below the knee is unchanged.
inline double curve(double x,double ceiling,double knee) noexcept {
 const double a=std::abs(x),k=std::clamp(knee,0.,.95);
 if(k<1.e-9)return std::clamp(x,-ceiling,ceiling);
 const double lo=ceiling*(1.-k),hi=ceiling*(1.+k);
 if(a<=lo)return x;
 if(a>=hi)return std::copysign(ceiling,x);
 const double u=(a-lo)/(hi-lo);
 return std::copysign(lo+(hi-lo)*(u-.5*u*u),x);
}
// US6337999 differential topology, with sparse 129-tap halfband FIR stages.
// The cascade has 128*(1-1/M) base-rate samples of round-trip group delay;
// correction-only padding makes every quality exactly 128 samples.
class Differential {
public:
 static constexpr int delay=128;
 void prepare() noexcept {
  double sum=0.;const double normal=bessel0(10.5);
  for(int i=0;i<64;++i){const double at=2.*i+1.-64.;const double z=at/64.;
   odd[static_cast<size_t>(i)]=.5*sinc(.5*at)*bessel0(10.5*std::sqrt(std::max(0.,1.-z*z)))/normal;sum+=odd[static_cast<size_t>(i)];}
  for(auto& h:odd)h*=.5/sum;
  reset();
 }
 void reset() noexcept {for(auto& x:up)x.reset();for(auto& x:down)x.reset();dry.reset();padding.reset();quality=-1;lastReduction=0.;ratio=1.;}
 void begin(double x,int q) noexcept {
  q=std::clamp(q,0,5);if(q!=quality){for(auto& h:up)h.reset();for(auto& h:down)h.reset();padding.reset();quality=q;}
  high[0]=x;ratio=1.;int count=1;
  for(int stage=0;stage<=quality;++stage){auto& state=up[static_cast<size_t>(stage)];
   for(int i=0;i<count;++i){state.push(high[static_cast<size_t>(i)]);const auto* history=state.data.data()+state.pos+129;
    scratch[static_cast<size_t>(2*i)]=history[-32];double value=0.;for(int j=0;j<64;++j)value+=odd[static_cast<size_t>(j)]*history[-j];scratch[static_cast<size_t>(2*i+1)]=2.*value;state.advance();}
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
   for(int i=0;i<count;i+=2){state.push(errors[static_cast<size_t>(i)]);const auto* history=state.data.data()+state.pos+129;
    double value=.5*history[-64];for(int j=0;j<64;++j)value+=odd[static_cast<size_t>(j)]*history[-(2*j+1)];scratch[static_cast<size_t>(i/2)]=value;state.advance();state.push(errors[static_cast<size_t>(i+1)]);state.advance();}
   count/=2;std::copy_n(scratch.begin(),count,errors.begin());
  }
  const double removed=padding.push(errors[0],128/factor());delayed=dry.push(x,delay);lastReduction=gainDb(ratio);return delayed-removed;
 }
 double original() const noexcept {return delayed;}
 double reduction() const noexcept {return lastReduction;}
private:
 struct State {
  std::array<double,258> data{};int pos=0;
  void reset() noexcept {data.fill(0.);pos=0;}
  void push(double x) noexcept {data[static_cast<size_t>(pos)]=data[static_cast<size_t>(pos+129)]=x;}
  void advance() noexcept {if(++pos==129)pos=0;}
 };
 std::array<State,6> up{},down{};std::array<double,64> odd{};std::array<double,64> high{},errors{},scratch{};
 FixedDelay<256> dry,padding;int quality=-1;double delayed=0.,lastReduction=0.,ratio=1.;
};

struct Biquad {
 double b0=1.,b1=0.,b2=0.,a1=0.,a2=0.,z1=0.,z2=0.;
 void lowpass(double hz,double sr) noexcept {
  const double w=2.*pi*hz/sr,c=std::cos(w),s=std::sin(w),alpha=s/std::sqrt(2.),a0=1.+alpha;
  b0=(1.-c)*.5/a0;b1=(1.-c)/a0;b2=b0;a1=-2.*c/a0;a2=(1.-alpha)/a0;reset();
 }
 double process(double x) noexcept {const double y=b0*x+z1;z1=b1*x-a1*y+z2;z2=b2*x-a2*y;return y;}
 void reset() noexcept {z1=z2=0.;}
};
// Perception controls only a tiny near-threshold knee; it never applies a
// broadband gain envelope. A separate low-onset detector is retained only for diagnostic tests.
class Perception {
public:
 static constexpr int latency=512;
 void prepare(double sr) noexcept {
  sampleRate=sr;low1.lowpass(100.,sr);low2.lowpass(100.,sr);body.lowpass(450.,sr);belowBody.lowpass(140.,sr);
  for(int i=0;i<512;++i)window[static_cast<size_t>(i)]=.5-.5*std::cos(2.*pi*i/512.);
  fastPole=pole(1./(2.*pi*.006),sr);slowPole=pole(1./(2.*pi*.12),sr);decay=std::exp(-1./(.055*sr));reset();
 }
 void reset() noexcept {samples.fill(0.);previous.fill(0.);clock=0;tonality=attack=gate=confidence=lowFast=lowSlow=bodyFast=0.;low1.reset();low2.reset();body.reset();belowBody.reset();}
 void process(double x) noexcept {
  const double low=low2.process(low1.process(x));const double mid=body.process(x)-belowBody.process(x);
  lowFast=low*low+(lowFast-low*low)*fastPole;lowSlow=low*low+(lowSlow-low*low)*slowPole;
  bodyFast=mid*mid+(bodyFast-mid*mid)*fastPole;
  const double dominance=std::clamp((lowFast-.8*bodyFast)/(lowFast+bodyFast+1.e-12),0.,1.);
  const double onset=std::clamp((lowFast/(lowSlow+1.e-9)-1.9)*.45,0.,1.);
  gate=std::max(gate*decay,dominance*onset);
  confidence=gate*dominance;
  samples[static_cast<size_t>(clock%512)]=x;++clock;if(clock%128!=0)return;
  for(int i=0;i<512;++i)spectrum[static_cast<size_t>(i)]={samples[static_cast<size_t>((clock+static_cast<std::uint64_t>(i))%512)]*window[static_cast<size_t>(i)],0.};
  fft.run(spectrum);double energy=1.e-20,strongest=0.,flux=0.;
  for(int i=1;i<256;++i){const auto j=static_cast<size_t>(i);const double power=std::norm(spectrum[j]);energy+=power;strongest=std::max(strongest,power);flux+=std::max(0.,power-previous[j]);previous[j]=power;}
  tonality=std::clamp(strongest/energy*1.5,0.,1.);attack=std::clamp(flux/energy,0.,1.);
 }
 double knee() const noexcept {return .0025+.0125*tonality*(1.-attack);}
 double kick() const noexcept {return confidence;}
private:
 FFT512 fft;std::array<double,512> samples{},window{};std::array<double,256> previous{};std::array<std::complex<double>,512> spectrum{};
 Biquad low1,low2,body,belowBody;std::uint64_t clock=0;
 double sampleRate=48000.,fastPole=0.,slowPole=0.,decay=0.,lowFast=0.,lowSlow=0.,bodyFast=0.,gate=0.,confidence=0.,tonality=0.,attack=0.;
};
// Symmetric FIR used by cancellation and complementary band splitting.
// Its 512-sample centre is aligned with the main analysis delay.
class LinearPhaseLowpass {
public:
 static constexpr int length=1025,centre=512;
 void prepare(double sr,double hz=100.) noexcept {
  double sum=0.;const double cutoff=hz/sr,normal=bessel0(8.6);
  for(int i=0;i<length;++i){const double at=i-centre,z=at/centre;const double value=2.*cutoff*sinc(2.*cutoff*at)*bessel0(8.6*std::sqrt(std::max(0.,1.-z*z)))/normal;coeff[static_cast<size_t>(i)]=value;sum+=value;}
  for(auto& c:coeff)c/=sum;reset();
 }
 void reset() noexcept {history.fill(0.);pos=0;}
 double process(double x,bool enabled) noexcept {
  history[static_cast<size_t>(pos)]=history[static_cast<size_t>(pos+length)]=x;double y=0.;
  if(enabled){const auto* h=history.data()+pos+length;y=coeff[centre]*h[-centre];for(int i=0;i<centre;++i)y+=coeff[static_cast<size_t>(i)]*(h[-i]+h[-(length-1-i)]);}
  if(++pos==length)pos=0;return y;
 }
private:std::array<double,length> coeff{};std::array<double,2*length> history{};int pos=0;
};
// Retrospective half-wave scaling with bounded lookahead. Never normalises up.
// A completed half-wave gets one gain, preserving its internal sample ratios.
class LowShape {
public:
 void reset() noexcept {audio.fill(0.);gains.fill(1.);clock=start=0;previous=peak=fullPeak=0.;lastGain=1.;}
 double process(double x,int delay,double threshold,double fallback,double full) noexcept {
  const auto slot=static_cast<size_t>(clock%capacity);audio[slot]=x;gains[slot]=fallback;
  const bool crossing=(x>=0.)!=(previous>=0.);
  if(crossing){const double g=std::min(1.,std::min(threshold*.90/std::max(1.e-12,peak),threshold*.95/std::max(1.e-12,fullPeak)));
   if(clock-start<static_cast<std::uint64_t>(delay))for(auto k=start;k<clock;++k)gains[static_cast<size_t>(k%capacity)]=g;
   start=clock;peak=fullPeak=0.;}
  peak=std::max(peak,std::abs(x));fullPeak=std::max(fullPeak,std::abs(full));previous=x;
  const auto read=static_cast<size_t>((clock+capacity-delay)%capacity);++clock;
  lastGain=gains[read];return audio[read]*lastGain;
 }
double gain() const noexcept {return lastGain;}
private:static constexpr int capacity=32768;std::array<double,capacity> audio{},gains{};std::uint64_t clock=0,start=0;double previous=0.,peak=0.,fullPeak=0.,lastGain=1.;
};
// Windowed FIR Hilbert transform; paired signal/control transforms form a
// lower-sideband cancellation experiment inspired by US6205225B1.
class Hilbert129 {
public:
 void prepare() noexcept {for(int i=0;i<129;++i){const int k=i-64;coeff[i]=(k!=0&&k%2!=0?2./(pi*k):0.)*(.5-.5*std::cos(2.*pi*i/128.));}reset();}
 void reset() noexcept {history.fill(0.);pos=0;}
 double process(double x) noexcept {history[pos]=history[pos+129]=x;double y=0.;const auto* h=history.data()+pos+129;for(int i=1;i<129;i+=2)y+=coeff[i]*h[-i];if(++pos==129)pos=0;return y;}
private:std::array<double,129> coeff{};std::array<double,258> history{};int pos=0;
};
class Engine {
public:
 void prepare(double sr) noexcept {
  sampleRate=std::clamp(finite(sr,48000.),8000.,384000.);
  for(auto& bank:differential)for(auto& d:bank)d.prepare();
  for(auto& p:perception)p.prepare(sampleRate);
  for(auto& bank:recoveryUpsampler)for(auto& d:bank)d.prepare();
  for(auto& c:correction)c.prepare(sampleRate,2000.);
  for(auto& c:lowFilter)c.prepare(sampleRate,100.);
  analysisLatency=512+std::max(128,int(std::ceil(sampleRate*.030)));
  for(auto& ch:imdHilbert)for(auto& h:ch)h.prepare();
  for(auto& c:imdFilter)c.prepare(sampleRate,2000.);
  
  for(auto& bank:bandUpsampler)for(auto& ch:bank)for(auto& d:ch)d.prepare();
  for(auto& p:probe)p.prepare();
  for(auto& ch:lowDetector)for(auto& f:ch)f.lowpass(100.,sampleRate);
  smoothPole=std::exp(-1./(.010*sampleRate));kneePole=std::exp(-1./(.002*sampleRate));reset();
 }
 void reset() noexcept {
  for(auto& bank:differential)for(auto& d:bank)d.reset();
  for(auto& p:perception)p.reset();
  for(auto& bank:recoveryUpsampler)for(auto& d:bank)d.reset();
  for(auto& c:correction)c.reset();
  for(auto& c:lowFilter)c.reset();
  for(auto& bank:bandUpsampler)for(auto& ch:bank)for(auto& d:ch)d.reset();
  for(auto& d:fullShapeDelay)d.reset();for(auto& d:lowDelay)d.reset();for(auto& d:lowShape)d.reset();
  for(auto& ch:imdHilbert)for(auto& h:ch)h.reset();
  for(auto& d:imdDelay)d.reset();for(auto& d:recoveryDelay)d.reset();for(auto& c:imdFilter)c.reset();
  lowPeak=lowGain=totalPeak=fullGain=dominance={};lowGain.fill(1.);fullGain.fill(1.);modeBlend.fill(0.);
  for(auto& p:probe)p.reset();
  for(auto& ch:lowDetector)for(auto& f:ch)f.reset();
  for(auto& d:dryDelay)d.reset();
  for(auto& d:analysisDelay)d.reset();
  controls={};primed=false;wet=1.;adaptiveKnee=0.;activeBank=0;activeQuality=3;nextQuality=3;qualityFade=0;
 }
 int latency() const noexcept {return Differential::delay+analysisLatency;}
 Result process(double l,double r,Settings target,int channels=2) noexcept {
  sanitize(target);if(!primed){controls=target;wet=target.bypass?0.:1.;activeQuality=target.quality;modeBlend[static_cast<size_t>(target.mode)]=1.;primed=true;}
  smooth(target);const double rawL=std::clamp(finite(l),-64.,64.),rawR=channels==1?rawL:std::clamp(finite(r),-64.,64.);
  const double dryL=dryDelay[0].push(rawL,latency()),dryR=dryDelay[1].push(rawR,latency());
  const double inGain=dbGain(controls.inDb),threshold=dbGain(controls.ceilingDb);
  for(int m=0;m<3;++m)modeBlend[static_cast<size_t>(m)]=(target.mode==m?1.:0.)+(modeBlend[static_cast<size_t>(m)]-(target.mode==m?1.:0.))*smoothPole;
  const bool bands=controls.bass>1.e-6;
  const bool cancel=modeBlend[1]>1.e-8;
  std::array<double,2> input{rawL*inGain,rawR*inGain},processed{},original{},kick{},recovery{},low{},unprotectedLow{},shapeGain{};
  for(int c=0;c<2;++c){const auto j=static_cast<size_t>(c);perception[j].process(input[j]);kick[j]=perception[j].kick();
   const double lf=lowFilter[j].process(input[j],bands);
   // FIR output leads the audio path by 128 samples. A peak hold spans LF cycles;
   // attack smoothing therefore happens before the aligned bass peak arrives.
   const double detectedLow=lowDetector[j][1].process(lowDetector[j][0].process(input[j]));
   lowPeak[j]=std::max(std::abs(detectedLow),lowPeak[j]*std::exp(-1./(.120*sampleRate)));
   totalPeak[j]=std::max(std::abs(input[j]),totalPeak[j]*std::exp(-1./(.120*sampleRate)));
   const double fullWant=std::min(1.,threshold*.95/std::max(1.e-12,totalPeak[j]));
   fullGain[j]=fullWant+(fullGain[j]-fullWant)*std::exp(-1./((fullWant<fullGain[j]?.0003:.090)*sampleRate));
   const double dom=std::clamp((lowPeak[j]/std::max(1.e-12,totalPeak[j])-.20)/.28,0.,1.);
   dominance[j]=dom+(dominance[j]-dom)*std::exp(-1./(.002*sampleRate));
   const double want=std::min(1.,threshold*.72/std::max(1.e-12,lowPeak[j]));
   const double tau=want<lowGain[j]?.0003:.090;
   lowGain[j]=want+(lowGain[j]-want)*std::exp(-1./(tau*sampleRate));
   low[j]=lowShape[j].process(lf,analysisLatency-512,threshold,lowGain[j],fullShapeDelay[j].push(input[j],512));shapeGain[j]=lowShape[j].gain();
   unprotectedLow[j]=lowDelay[j].push(lf,analysisLatency-512);
   const double gx=std::abs(input[j])>1.e-12?curve(input[j],threshold,controls.knee*.0095)/input[j]:1.;
   const double imd=imdHilbert[j][0].process(input[j])*imdHilbert[j][1].process(gx)*(1.-dominance[j]);
   const double imdCorrection=imdFilter[j].process(imdDelay[j].push(imd,64),cancel&&target.imdClean);
   double removed=0.;auto& p=probe[j];p.begin(input[j],2);
   for(int ph=0;ph<p.factor();++ph){const double v=p.interpolate(ph);p.correct(v,curve(v,threshold,controls.knee*.0095),ph,threshold);}
   const double clipped=p.end(input[j]);removed=p.original()-clipped;
   recovery[j]=recoveryDelay[j].push(correction[j].process(removed,cancel)+(target.imdClean?-.35*imdCorrection:0.),analysisLatency-640);input[j]=analysisDelay[j].push(input[j],analysisLatency);}
  const double desired=controls.knee*.0095;adaptiveKnee=desired;
  if(qualityFade==0&&controls.quality!=activeQuality){nextQuality=controls.quality;for(auto& d:differential[static_cast<size_t>(1-activeBank)])d.reset();for(auto& d:recoveryUpsampler[static_cast<size_t>(1-activeBank)])d.reset();for(auto& ch:bandUpsampler[static_cast<size_t>(1-activeBank)])for(auto& d:ch)d.reset();qualityFade=1;}
  double reduction=0.,activity=0.;
  auto runBank=[&](int bank,int quality,std::array<double,2>& destination){
   auto& pair=differential[static_cast<size_t>(bank)];
   auto& recoveryPair=recoveryUpsampler[static_cast<size_t>(bank)];
   auto& bp=bandUpsampler[static_cast<size_t>(bank)];
   for(int c=0;c<2;++c){const auto j=static_cast<size_t>(c);pair[j].begin(input[j],quality);if(cancel)recoveryPair[j].begin(recovery[j],quality);
    if(bands){bp[j][0].begin(low[j],quality);bp[j][1].begin(unprotectedLow[j],quality);bp[j][2].begin(shapeGain[j],quality);}}
   for(int ph=0;ph<pair[0].factor();++ph){
    for(int c=0;c<2;++c){const auto j=static_cast<size_t>(c);const double x=pair[j].interpolate(ph);
     const double rawLow=bands?bp[j][1].interpolate(ph):0.,lf=bands?bp[j][0].interpolate(ph):0.;
     // Raw low band is recovered from the protected band using the same slow gain.
     const double base=curve(x,threshold,adaptiveKnee);
     const double headroom=std::max(threshold*.05,threshold-std::abs(lf));
     const double splitClip=lf+curve(x-rawLow,headroom,adaptiveKnee);
     const double protectedClip=splitClip+dominance[j]*(curve(x*std::clamp(bp[j][2].interpolate(ph),0.,1.),threshold,adaptiveKnee)-splitClip);
     const double protect=controls.bass*.01;
     std::array<double,3> y{base,base+(cancel?std::clamp(.8*recoveryPair[j].interpolate(ph),-threshold*.35,threshold*.35):0.),analog(x,threshold,adaptiveKnee)};
     double shaped=0.;for(int m=0;m<3;++m)shaped+=modeBlend[static_cast<size_t>(m)]*y[static_cast<size_t>(m)];
     // Protection replaces a portion of the selected nonlinear result, rather
     // than adding clipped bass back into another clipper.
     const double engaged=std::min(std::clamp((lowPeak[j]/threshold-.55)*4.,0.,1.),std::clamp((totalPeak[j]/threshold-1.)*4.,0.,1.));
     shaped+=protect*engaged*(protectedClip-shaped);
     pair[j].correct(x,shaped,ph,threshold);
     activity=std::max(activity,std::abs(x-shaped)/std::max(1.e-12,threshold));
    }
   }
   for(int c=0;c<2;++c){const auto j=static_cast<size_t>(c);destination[j]=pair[j].end(input[j]);reduction=std::max(reduction,pair[j].reduction());}
  };
  runBank(activeBank,activeQuality,processed);
  for(int c=0;c<2;++c)original[static_cast<size_t>(c)]=differential[static_cast<size_t>(activeBank)][static_cast<size_t>(c)].original();
  if(qualityFade>0){std::array<double,2> next{};runBank(1-activeBank,nextQuality,next);const double blend=std::clamp((qualityFade-256.)/512.,0.,1.);for(int c=0;c<2;++c){const auto j=static_cast<size_t>(c);processed[j]+=blend*(next[j]-processed[j]);}if(++qualityFade>768){activeBank=1-activeBank;activeQuality=nextQuality;qualityFade=0;}}
  const double outGain=dbGain(controls.outDb),wetTarget=target.bypass?0.:1.;wet=wetTarget+(wet-wetTarget)*std::exp(-1./(.005*sampleRate));
  if(std::abs(wet-wetTarget)<1.e-12)wet=wetTarget;
  // Explicit native-sample bound after reconstruction, BEFORE the final Output Gain.
  // This is a sample clip, not an ISP/true-peak limiter. Bypass remains raw.
  const double bound=threshold;
  const double preL=processed[0],preR=processed[1];
  const double wl=std::clamp(preL,-bound,bound)*outGain,wr=std::clamp(preR,-bound,bound)*outGain;
  reduction=std::max(reduction,std::max(0.,gainDb(std::max(std::abs(preL),std::abs(preR))/std::max(1.e-12,bound))));
  Result result;result.l=dryL+wet*(wl-dryL);result.r=dryR+wet*(wr-dryR);
  if(!target.bypass){result.l=std::clamp(result.l,-bound*outGain,bound*outGain);result.r=std::clamp(result.r,-bound*outGain,bound*outGain);}
  result.inputPeak=std::max(std::abs(rawL*inGain),std::abs(rawR*inGain));result.reductionDb=target.bypass?0.:reduction;result.clipActivity=activity;result.kickConfidence=std::max(kick[0],kick[1]);return result;
 }
private:
 static void sanitize(Settings& s) noexcept {
  auto clamp=[](double& x,double lo,double hi,double fallback){x=std::clamp(finite(x,fallback),lo,hi);};
  clamp(s.inDb,-24.,36.,0.);clamp(s.ceilingDb,-30.,0.,0.);clamp(s.outDb,-24.,12.,0.);clamp(s.bass,0.,100.,0.);s.quality=std::clamp(s.quality,0,5);s.mode=std::clamp(s.mode,0,2);clamp(s.knee,0.,100.,0.);
 }
 void smooth(const Settings& t) noexcept {auto move=[this](double& x,double y){x=y+(x-y)*smoothPole;};move(controls.inDb,t.inDb);move(controls.ceilingDb,t.ceilingDb);move(controls.outDb,t.outDb);move(controls.bass,t.bass);move(controls.knee,t.knee);controls.quality=t.quality;}
 static double analog(double x,double t,double k) noexcept {if(k<1.e-9)return std::clamp(x,-t,t);const double a=std::abs(x),lo=t*(1.-k);if(a<=lo)return x;return std::copysign(lo+t*k*std::tanh((a-lo)/(t*k)),x);}
 std::array<std::array<std::array<Differential,3>,2>,2> bandUpsampler;
 std::array<LinearPhaseLowpass,2> lowFilter;std::array<Delay,2> lowDelay;std::array<FixedDelay<1024>,2> fullShapeDelay;std::array<LowShape,2> lowShape;
 std::array<std::array<Hilbert129,2>,2> imdHilbert;std::array<FixedDelay<256>,2> imdDelay;std::array<Delay,2> recoveryDelay;std::array<LinearPhaseLowpass,2> imdFilter;
 std::array<std::array<Biquad,2>,2> lowDetector;
 std::array<double,2> lowPeak{},lowGain{},totalPeak{},fullGain{},dominance{};std::array<double,3> modeBlend{};
 std::array<std::array<Differential,2>,2> differential;std::array<Perception,2> perception;
 int analysisLatency=1952;
 std::array<std::array<Differential,2>,2> recoveryUpsampler;std::array<LinearPhaseLowpass,2> correction;std::array<Differential,2> probe;
 std::array<Delay,2> dryDelay,analysisDelay;Settings controls;
 double sampleRate=48000.,smoothPole=0.,kneePole=0.,wet=1.,adaptiveKnee=0.;bool primed=false;int activeBank=0,activeQuality=3,nextQuality=3,qualityFade=0;
};
}

#include "../Source/ClipDSP.h"
#include <fstream>
#include <iostream>
#include <memory>
int main(int argc,char** argv){
 const std::string folder=argc>1?argv[1]:"measurements";
 for(int q=0;q<6;++q)for(int protect:{0,100}){
  auto engine=std::make_unique<clip::Engine>();engine->prepare(48000.);clip::Settings s;s.quality=q;s.bass=protect;
  // Coherent two-tone + sine cases; output preserves a complete steady frame.
  for(int test=0;test<4;++test){engine->reset();const double f=test==0?997.:test==1?7000.:test==2?15000.:73.;
   std::ofstream file(folder+"/q"+std::to_string(q)+"-p"+std::to_string(protect)+"-t"+std::to_string(test)+".f64",std::ios::binary);
   if(!file){std::cerr<<"Create output folder first\n";return 1;}
   for(int i=0;i<65536+8192;++i){const double x=test==3?1.15*std::sin(2.*clip::pi*73.*i/48000.)+.25*std::sin(2.*clip::pi*7013.*i/48000.):1.45*std::sin(2.*clip::pi*f*i/48000.);
    const auto y=engine->process(x,x,s);if(i>=8192){const double data[]{x,y.l};file.write(reinterpret_cast<const char*>(data),sizeof(data));}}
  }
 }
 std::cout<<"Measurement audio exported\n";
}

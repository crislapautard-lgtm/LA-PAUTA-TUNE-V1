#pragma once
#include <array>
#include <cmath>
#include <vector>
namespace pauta {
using ScaleMask=std::array<bool,12>;
inline const char* const* noteNames(){ static const char* const n[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}; return n; }
struct ScaleType{ const char* name; std::vector<int> degrees; };
inline const std::vector<ScaleType>& scaleTypes(){
 static const std::vector<ScaleType> t={{"Mayor",{0,2,4,5,7,9,11}},{"Menor natural",{0,2,3,5,7,8,10}},{"Menor armónica",{0,2,3,5,7,8,11}},{"Menor melódica",{0,2,3,5,7,9,11}},{"Pentatónica mayor",{0,2,4,7,9}},{"Pentatónica menor",{0,3,5,7,10}},{"Blues",{0,3,5,6,7,10}},{"Dórica",{0,2,3,5,7,9,10}},{"Frigia",{0,1,3,5,7,8,10}},{"Lidia",{0,2,4,6,7,9,11}},{"Mixolidia",{0,2,4,5,7,9,10}},{"Cromática",{0,1,2,3,4,5,6,7,8,9,10,11}}};
 return t;
}
inline int mod12(int v){ return ((v%12)+12)%12; }
inline ScaleMask makeScaleMask(int key,int st){ ScaleMask m{}; auto &ts=scaleTypes(); if(st<0||st>=(int)ts.size()) st=0; for(int d:ts[st].degrees) m[mod12(key+d)]=true; return m; }
inline bool isInScale(int midi,const ScaleMask& m){ return m[mod12(midi)]; }
inline int nearestInScale(float midi,const ScaleMask& m){
 int base=(int)std::floor(midi+0.5f);
 for(int d=0;d<=6;d++){ int lo=base-d,hi=base+d; bool okLo=isInScale(lo,m),okHi=isInScale(hi,m);
  if(okLo&&okHi) return std::fabs((float)lo-midi)<=std::fabs((float)hi-midi)?lo:hi; if(okLo) return lo; if(okHi) return hi; }
 return base;
}
}

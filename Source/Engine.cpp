#include "Engine.h"
namespace pauta {
bool analyzePitch(const std::vector<float>& mono, double sr, F0Track& out, std::function<bool(float)> progress){
    out.sampleRate=sr; out.hopSeconds=0.01f;
    int hop=(int)std::max(1.0,sr*0.01);
    int win=(int)(sr*0.04);
    int n=(int)mono.size()/hop+1;
    out.f0.assign(n,0.f);
    for(int i=0;i<n;i++){
        if((i%50)==0 && progress && !progress((float)i/(float)n)) return false;
        int c=i*hop;
        if(c+win>=(int)mono.size()) break;
        float bestCorr=-1; int bestLag=0;
        int minLag=(int)(sr/400), maxLag=(int)(sr/60);
        for(int lag=minLag;lag<maxLag;lag++){
            float s=0,a=0,b=0;
            for(int j=0;j<win;j+=2){ float x=mono[c+j], y=mono[c+j+lag]; s+=x*y; a+=x*x; b+=y*y; }
            float corr=(a>1e-9&&b>1e-9)? s/std::sqrt(a*b):0;
            if(corr>bestCorr){bestCorr=corr;bestLag=lag;}
        }
        float energy=0; for(int j=0;j<win;j++) energy+=mono[c+j]*mono[c+j];
        if(bestCorr>0.75f && energy>1e-4f) out.f0[i]= (float)sr/(float)bestLag;
    }
    return true;
}
std::vector<Note> segmentNotes(const F0Track& t, int total){
    std::vector<Note> notes; int hop=(int)(t.sampleRate*t.hopSeconds);
    int i=0; int N=(int)t.f0.size();
    while(i<N){
        if(t.f0[i]<=0){ i++; continue; }
        int s=i; float sum=0; int cnt=0;
        while(i<N && t.f0[i]>0){ sum+=hzToMidi(t.f0[i]); cnt++; i++; }
        if(cnt>=4 && (i-s)*hop>8000){
            Note n; n.start=s*hop; n.end=std::min(total,i*hop); n.detectedMidi=sum/cnt; n.targetMidi=n.detectedMidi; notes.push_back(n);
        }
    }
    return notes;
}
bool splitNote(std::vector<Note>& notes,int idx,int at,const F0Track&){
    if(idx<0||idx>=(int)notes.size()) return false;
    Note o=notes[idx];
    int realAt=at+o.shift;
    if(realAt<=o.start+2000||realAt>=o.end-2000) return false;
    Note a=o,b=o; a.end=realAt; b.start=realAt;
    notes[idx]=a; notes.insert(notes.begin()+idx+1,b); return true;
}
std::vector<float> render(const std::vector<float>& mono,const F0Track&,const std::vector<Note>& notes){
    std::vector<float> out(mono.size(),0.f);
    // Copia base
    out=mono;
    // Pitch shift simple por nota: resampleo con ratio segun diferencia de tono
    for(auto &n: notes){
        if(std::abs(n.targetMidi-n.detectedMidi)<0.01f && n.shift==0) continue;
        float ratio=std::pow(2.f,(n.targetMidi-n.detectedMidi)/12.f);
        // Si hay variacion, mezcla entre original y corregido
        float v=n.variation;
        int s=n.start, e=n.end;
        for(int i=s;i<e;i++){
            int srcPos=n.start+n.shift+(int)((i-s)*ratio);
            float sample=(srcPos>=0&&srcPos<(int)mono.size())?mono[srcPos]:0.f;
            float dry=mono[i];
            out[i]=dry*(1.f-v)+sample*v;
        }
        // Time shift: mover bloque si hay shift y no se solapa
        if(n.shift!=0){
            // ya aplicado arriba via srcPos, no duplicar
        }
    }
    return out;
}
}

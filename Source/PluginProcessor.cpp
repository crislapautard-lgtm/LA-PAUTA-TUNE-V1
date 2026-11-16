#include "PluginProcessor.h"
#include "PluginEditor.h"
struct LaPautaTuneProcessor::LoadResult{int generation=0; bool ok=false; juce::String error,path,name; double sampleRate=44100; int channels=1; std::shared_ptr<std::vector<float>> mono; std::shared_ptr<pauta::F0Track> track; std::vector<pauta::Note> notes; std::vector<float> peaks;};
class LaPautaTuneProcessor::AnalysisThread : public juce::Thread{
public: AnalysisThread(LaPautaTuneProcessor&o,juce::File f,std::vector<pauta::Note> s,int g):juce::Thread("La Pauta Tune analisis"),owner(o),file(std::move(f)),savedNotes(std::move(s)),generation(g){}
 void run() override{
  auto res=std::make_shared<LoadResult>(); res->generation=generation; res->path=file.getFullPathName(); res->name=file.getFileNameWithoutExtension();
  juce::AudioFormatManager fm; fm.registerBasicFormats();
  std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(file));
  if(r==nullptr||r->lengthInSamples<=0){res->error="No se pudo abrir el audio. Usa WAV, AIFF, FLAC, MP3 u OGG."; post(res); return;}
  res->sampleRate=r->sampleRate; res->channels=(int)r->numChannels; int64_t total=r->lengthInSamples;
  if(total>(int64_t)60*60*(int64_t)r->sampleRate){res->error="Audio demasiado largo (max 60 min)."; post(res); return;}
  auto mono=std::make_shared<std::vector<float>>((size_t)total,0.f);
  { const int chunk=65536; juce::AudioBuffer<float> buf((int)r->numChannels,chunk);
    for(int64_t pos=0;pos<total;pos+=chunk){ if(threadShouldExit()) return; int len=(int)std::min<int64_t>(chunk,total-pos); r->read(&buf,0,len,pos,true,true);
     for(int i=0;i<len;i++){ float s=0; for(int c=0;c<buf.getNumChannels();c++) s+=buf.getSample(c,i); (*mono)[pos+i]=s/(float)buf.getNumChannels(); } } }
  { size_t nb=(mono->size()+peakBlock-1)/peakBlock; res->peaks.assign(nb,0.f); float gmax=1e-6f;
    for(size_t b=0;b<nb;b++){ size_t a=b*peakBlock,e=std::min(mono->size(),a+peakBlock); float m=0; for(size_t i=a;i<e;i++) m=std::max(m,std::fabs((*mono)[i])); res->peaks[b]=m; gmax=std::max(gmax,m);} for(auto &p:res->peaks) p/=gmax; }
  auto trk=std::make_shared<pauta::F0Track>();
  bool fin=pauta::analyzePitch(*mono,res->sampleRate,*trk,[this](float p){ owner.loadProgress.store(p); return !threadShouldExit(); });
  if(!fin||threadShouldExit()) return;
  res->mono=mono; res->track=trk; res->notes=savedNotes.empty()?pauta::segmentNotes(*trk,(int)mono->size()):savedNotes; res->ok=true; post(res);
 }
private: void post(std::shared_ptr<LoadResult> res){ if(threadShouldExit()) return; juce::WeakReference<LaPautaTuneProcessor> w(&owner); juce::MessageManager::callAsync([w,res]{ if(w!=nullptr) w->installProject(*res); }); }
 LaPautaTuneProcessor& owner; juce::File file; std::vector<pauta::Note> savedNotes; int generation;
};
class LaPautaTuneProcessor::RenderThread : public juce::Thread{
public: explicit RenderThread(LaPautaTuneProcessor&o):juce::Thread("La Pauta Tune render"),owner(o){}
 void run() override{ while(!threadShouldExit()){ wait(-1); while(dirty.exchange(false)&&!threadShouldExit()){ std::shared_ptr<const std::vector<float>> m; std::shared_ptr<const pauta::F0Track> t; std::vector<pauta::Note> n; { const juce::ScopedLock sl(owner.renderLock); m=owner.renderMono; t=owner.renderTrack; n=owner.renderNotes; } if(!m||!t) continue; auto result=std::make_shared<const std::vector<float>>(pauta::render(*m,*t,n)); const juce::SpinLock::ScopedLockType sl(owner.bufLock); owner.playBuf=result; } } }
 std::atomic<bool> dirty{false}; private: LaPautaTuneProcessor& owner;
};
LaPautaTuneProcessor::LaPautaTuneProcessor():juce::AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)){ renderThread=std::make_unique<RenderThread>(*this); renderThread->startThread(); }
LaPautaTuneProcessor::~LaPautaTuneProcessor(){ if(analysisThread) analysisThread->stopThread(5000); renderThread->signalThreadShouldExit(); renderThread->notify(); renderThread->stopThread(5000); }
void LaPautaTuneProcessor::prepareToPlay(double sr,int){ hostSampleRate=sr>0?sr:44100; }
bool LaPautaTuneProcessor::isBusesLayoutSupported(const BusesLayout& l) const { auto o=l.getMainOutputChannelSet(); return o==juce::AudioChannelSet::stereo()||o==juce::AudioChannelSet::mono(); }
void LaPautaTuneProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi){
 juce::ScopedNoDenormals d; midi.clear(); buffer.clear();
 bool hostPlaying=false; double hostSeconds=0;
 if(auto* ph=getPlayHead()) if(auto pos=ph->getPosition()){ hostPlaying=pos->getIsPlaying(); if(auto t=pos->getTimeInSeconds()) hostSeconds=*t; if(auto b=pos->getBpm()) if(*b>0) bpm.store(*b); }
 std::shared_ptr<const std::vector<float>> buf; { const juce::SpinLock::ScopedLockType sl(bufLock); buf=playBuf; }
 double srcSR=srcSampleRate; int n=buffer.getNumSamples(); double ratio=srcSR/hostSampleRate;
 double pos=0; bool active=false;
 if(hostPlaying){ pos=hostSeconds*srcSR; active=true; } else if(internalPlaying.load()){ pos=internalPos.load(); active=true; }
 if(buf&&active&&!buf->empty()){
  auto &v=*buf; double size=(double)v.size(); int nch=buffer.getNumChannels();
  for(int i=0;i<n;i++){ double p=pos+(double)i*ratio; if(p<0||p>=size-1) continue; int i0=(int)p; float fr=(float)(p-i0); float s=v[i0]+(v[i0+1]-v[i0])*fr; for(int c=0;c<nch;c++) buffer.setSample(c,i,s); }
  if(!hostPlaying){ double np=pos+(double)n*ratio; if(np>=size-1){ internalPlaying=false; np=0; } internalPos.store(np); }
 } else if(!hostPlaying&&internalPlaying.load()) internalPlaying=false;
 playPosSeconds.store(hostPlaying?hostSeconds:internalPos.load()/srcSR);
}
void LaPautaTuneProcessor::setPlayPositionSeconds(double s){ s=std::max(0.0,s); internalPos.store(s*srcSampleRate); playPosSeconds.store(s); }
void LaPautaTuneProcessor::loadFile(const juce::File& f){ if(analysisThread) analysisThread->stopThread(5000); ++loadGeneration; loading=true; loadProgress=0; messageText="Analizando la voz..."; sendChangeMessage(); analysisThread=std::make_unique<AnalysisThread>(*this,f,std::vector<pauta::Note>(),loadGeneration); analysisThread->startThread(); }
void LaPautaTuneProcessor::restoreState(const juce::String& path,std::vector<pauta::Note> saved){ juce::File f(path); if(!f.existsAsFile()){ messageText="No se encontro la voz guardada: "+path; sendChangeMessage(); return; } if(analysisThread) analysisThread->stopThread(5000); ++loadGeneration; loading=true; loadProgress=0; messageText="Analizando la voz..."; sendChangeMessage(); analysisThread=std::make_unique<AnalysisThread>(*this,f,std::move(saved),loadGeneration); analysisThread->startThread(); }
void LaPautaTuneProcessor::installProject(LoadResult& r){ if(r.generation!=loadGeneration) return; loading=false; if(!r.ok){ messageText=r.error; sendChangeMessage(); return; } mono=r.mono; track=r.track; notes=std::move(r.notes); peaks=std::move(r.peaks); srcSampleRate=r.sampleRate; srcChannels=r.channels; projectName=r.name; projectPath=r.path; undoStack.clear(); internalPlaying=false; setPlayPositionSeconds(0); { const juce::SpinLock::ScopedLockType sl(bufLock); playBuf.reset(); } messageText=projectName+" - "+juce::String((int)notes.size())+" notas detectadas"; requestRender(); sendChangeMessage(); }
void LaPautaTuneProcessor::requestRender(){ if(!mono||!track) return; { const juce::ScopedLock sl(renderLock); renderMono=mono; renderTrack=track; renderNotes=notes; } renderThread->dirty=true; renderThread->notify(); }
void LaPautaTuneProcessor::pushUndo(){ undoStack.push_back(notes); if(undoStack.size()>100) undoStack.erase(undoStack.begin()); }
void LaPautaTuneProcessor::undo(){ if(undoStack.empty()) return; notes=std::move(undoStack.back()); undoStack.pop_back(); requestRender(); sendChangeMessage(); }
bool LaPautaTuneProcessor::cutNote(int idx,int at){ if(!track) return false; pushUndo(); if(!pauta::splitNote(notes,idx,at,*track)){ undoStack.pop_back(); return false; } requestRender(); return true; }
bool LaPautaTuneProcessor::anySelected() const { for(auto &n:notes) if(n.selected) return true; return false; }
void LaPautaTuneProcessor::selectAll(bool s){ for(auto &n:notes) n.selected=s; }
void LaPautaTuneProcessor::centerNotes(){ auto mask=getScaleMask(); bool only=anySelected(); for(auto &n:notes){ if(only&&!n.selected) continue; float snap=(float)pauta::nearestInScale(n.detectedMidi,mask); n.targetMidi=n.detectedMidi+(snap-n.detectedMidi)*centerAmount; } requestRender(); }
void LaPautaTuneProcessor::resetNotes(){ bool only=anySelected(); for(auto &n:notes){ if(only&&!n.selected) continue; n.targetMidi=n.detectedMidi; n.variation=1.f; n.shift=0; } requestRender(); }
void LaPautaTuneProcessor::applyVariation(float v){ bool only=anySelected(); for(auto &n:notes) if(!only||n.selected) n.variation=v; requestRender(); }
juce::File LaPautaTuneProcessor::exportCorrectedAudio(){
 if(!mono||!track) return {}; auto out=pauta::render(*mono,*track,notes);
 auto folder=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("La Pauta Tune"); if(!folder.createDirectory().wasOk()) folder=juce::File::getSpecialLocation(juce::File::tempDirectory);
 auto safe=juce::File::createLegalFileName(projectName.isEmpty()?"voz":projectName); auto file=folder.getChildFile(safe+"_afinada_"+juce::Time::getCurrentTime().formatted("%H%M%S")+".wav");
 int nch=std::max(1,srcChannels); juce::AudioBuffer<float> buf(nch,(int)out.size()); for(int c=0;c<nch;c++) buf.copyFrom(c,0,out.data(),(int)out.size());
 juce::WavAudioFormat wav; std::unique_ptr<juce::FileOutputStream> st(file.createOutputStream()); if(!st) return {}; std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(st.get(),srcSampleRate,(unsigned)nch,24,{},0)); if(!w) return {}; st.release(); w->writeFromAudioSampleBuffer(buf,0,buf.getNumSamples()); w.reset(); messageText="Guardado: "+file.getFullPathName(); sendChangeMessage(); return file;
}
juce::File LaPautaTuneProcessor::exportForDragAndDrop(){ auto f=exportCorrectedAudio(); return f; }
void LaPautaTuneProcessor::getStateInformation(juce::MemoryBlock& dest){ juce::XmlElement xml("LaPautaTune"); xml.setAttribute("file",projectPath); xml.setAttribute("key",key); xml.setAttribute("scale",scaleType); xml.setAttribute("lock",lockScale); xml.setAttribute("grid",gridIndex); xml.setAttribute("center",(double)centerAmount); xml.setAttribute("bpm",bpm.load()); for(auto &n:notes){ auto* e=xml.createNewChildElement("Note"); e->setAttribute("s",n.start); e->setAttribute("e",n.end); e->setAttribute("d",(double)n.detectedMidi); e->setAttribute("t",(double)n.targetMidi); e->setAttribute("v",(double)n.variation); e->setAttribute("sh",n.shift); } copyXmlToBinary(xml,dest); }
void LaPautaTuneProcessor::setStateInformation(const void* data,int size){ auto xml=getXmlFromBinary(data,size); if(!xml||!xml->hasTagName("LaPautaTune")) return; auto path=xml->getStringAttribute("file"); int k=xml->getIntAttribute("key",0),sc=xml->getIntAttribute("scale",0),gr=xml->getIntAttribute("grid",2); bool lk=xml->getBoolAttribute("lock",false); float ca=(float)xml->getDoubleAttribute("center",1.0); double bp=xml->getDoubleAttribute("bpm",120); std::vector<pauta::Note> saved; for(auto* e:xml->getChildWithTagNameIterator("Note")){ pauta::Note n; n.start=e->getIntAttribute("s"); n.end=e->getIntAttribute("e"); n.detectedMidi=(float)e->getDoubleAttribute("d",60); n.targetMidi=(float)e->getDoubleAttribute("t",60); n.variation=(float)e->getDoubleAttribute("v",1); n.shift=e->getIntAttribute("sh"); saved.push_back(n); } juce::WeakReference<LaPautaTuneProcessor> weak(this); juce::MessageManager::callAsync([weak,path,k,sc,lk,gr,ca,bp,saved]{ if(weak==nullptr) return; weak->key=k; weak->scaleType=sc; weak->lockScale=lk; weak->gridIndex=gr; weak->centerAmount=ca; weak->bpm.store(bp); if(path.isNotEmpty()) weak->restoreState(path,saved); else weak->sendChangeMessage(); }); }
juce::AudioProcessorEditor* LaPautaTuneProcessor::createEditor(){ return new LaPautaTuneEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){ return new LaPautaTuneProcessor(); }

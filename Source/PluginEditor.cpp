#include "PluginEditor.h"
static void styleBtn(juce::TextButton& b,bool active=false){ b.setColour(juce::TextButton::buttonColourId, active?juce::Colour(0xffe5953d):juce::Colour(0xff2a3642)); b.setColour(juce::TextButton::textColourOffId, active?juce::Colours::black:juce::Colours::white); b.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xffe5953d)); }
static void styleBox(juce::ComboBox& b){ b.setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff141c22)); b.setColour(juce::ComboBox::textColourId,juce::Colours::white); b.setColour(juce::ComboBox::arrowColourId,juce::Colour(0xffe5953d)); }
LaPautaTuneEditor::LaPautaTuneEditor(LaPautaTuneProcessor& p):juce::AudioProcessorEditor(&p),proc(p),roll(p){
 setSize(1100,650); setResizable(true,true); setResizeLimits(800,450,1800,1000);
 for(auto* b:{&loadBtn,&playBtn,&stopBtn,&exportBtn,&tonoBtn,&tiempoBtn,&corteBtn,&lockBtn,&centerBtn,&resetBtn,&undoBtn}){ addAndMakeVisible(b); styleBtn(*b); }
 styleBtn(tonoBtn,true); styleBtn(lockBtn,true);
 for(auto* c:{&keyBox,&scaleBox,&gridBox,&bpmBox}){ addAndMakeVisible(c); styleBox(*c); }
 const char* keys[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
 for(int i=0;i<12;i++) keyBox.addItem(keys[i],i+1); keyBox.setSelectedId(proc.key+1,juce::dontSendNotification);
 for(auto &s:pauta::scaleTypes()) scaleBox.addItem(s.name,scaleBox.getNumItems()+1); scaleBox.setSelectedId(proc.scaleType+1,juce::dontSendNotification);
 gridBox.addItem("Off",1); gridBox.addItem("1/1",2); gridBox.addItem("1/2",3); gridBox.addItem("1/4",4); gridBox.addItem("1/8",5); gridBox.addItem("1/16",6); gridBox.setSelectedId(proc.gridIndex+1,juce::dontSendNotification);
 for(double b: {60.,70.,80.,88.,90.,100.,110.,120.,130.,140.}) bpmBox.addItem(juce::String(b,1),bpmBox.getNumItems()+1); bpmBox.setText(juce::String(proc.bpm.load(),1),juce::dontSendNotification); bpmBox.setEditableText(true);
 addAndMakeVisible(zoomSlider); zoomSlider.setSliderStyle(juce::Slider::LinearHorizontal); zoomSlider.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0); zoomSlider.setRange(15,1500,1); zoomSlider.setValue(100); zoomSlider.setColour(juce::Slider::thumbColourId,juce::Colours::white); zoomSlider.setColour(juce::Slider::trackColourId,juce::Colour(0xffe5953d));
 for(auto* k:{&centroKnob,&variacionKnob}){ addAndMakeVisible(k); k->setSliderStyle(juce::Slider::RotaryVerticalDrag); k->setTextBoxStyle(juce::Slider::NoTextBox,false,0,0); k->setRange(0,2,0.01); k->setValue(1.0); k->setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(0xffe5953d)); k->setColour(juce::Slider::thumbColourId,juce::Colours::white); k->setDoubleClickReturnValue(true,1.0); }
 addAndMakeVisible(centroLabel); centroLabel.setText("100 %\nCentro",juce::dontSendNotification); centroLabel.setJustificationType(juce::Justification::centred); centroLabel.setColour(juce::Label::textColourId,juce::Colours::white);
 addAndMakeVisible(variacionLabel); variacionLabel.setText("100 %\nVariación",juce::dontSendNotification); variacionLabel.setJustificationType(juce::Justification::centred); variacionLabel.setColour(juce::Label::textColourId,juce::Colours::white);
 addAndMakeVisible(titleLabel); titleLabel.setFont(juce::FontOptions(18.f,juce::Font::bold)); titleLabel.setColour(juce::Label::textColourId,juce::Colour(0xffe5953d)); titleLabel.setJustificationType(juce::Justification::centredRight);
 addAndMakeVisible(statusLabel); statusLabel.setColour(juce::Label::textColourId,juce::Colour(0xffc9d1d9));
 addAndMakeVisible(dragBtn); dragBtn.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff2a3642)); dragBtn.setColour(juce::TextButton::textColourOffId,juce::Colours::white);
 addAndMakeVisible(roll);
 roll.onZoomChanged=[this](double v){ zoomSlider.setValue(v,juce::dontSendNotification); };
 roll.onSelectionChanged=[this]{ refreshFromProcessor(); };
 loadBtn.onClick=[this]{ chooser.launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this](const juce::FileChooser& c){ if(c.getResult().existsAsFile()) proc.loadFile(c.getResult()); }); };
 playBtn.onClick=[this]{ if(proc.hasProject()){ proc.setPlaying(true); } };
 stopBtn.onClick=[this]{ proc.setPlaying(false); proc.setPlayPositionSeconds(0); };
 exportBtn.onClick=[this]{ proc.exportCorrectedAudio(); };
 tonoBtn.onClick=[this]{ setTool(PianoRoll::Tool::pitch); }; tiempoBtn.onClick=[this]{ setTool(PianoRoll::Tool::time); }; corteBtn.onClick=[this]{ setTool(PianoRoll::Tool::cut); };
 lockBtn.onClick=[this]{ proc.lockScale=!proc.lockScale; styleBtn(lockBtn,proc.lockScale); roll.repaint(); };
 centerBtn.onClick=[this]{ proc.centerNotes(); }; resetBtn.onClick=[this]{ proc.resetNotes(); }; undoBtn.onClick=[this]{ proc.undo(); };
 keyBox.onChange=[this]{ proc.key=keyBox.getSelectedId()-1; proc.requestRender(); roll.repaint(); };
 scaleBox.onChange=[this]{ proc.scaleType=scaleBox.getSelectedId()-1; proc.requestRender(); roll.repaint(); };
 gridBox.onChange=[this]{ proc.gridIndex=gridBox.getSelectedId()-1; roll.repaint(); };
 bpmBox.onChange=[this]{ auto t=bpmBox.getText().getDoubleValue(); if(t>20) proc.bpm.store(t); };
 zoomSlider.onValueChange=[this]{ roll.setPixelsPerSecond(zoomSlider.getValue()); };
 centroKnob.onValueChange=[this]{ proc.centerAmount=(float)centroKnob.getValue(); centroLabel.setText(juce::String((int)std::round(centroKnob.getValue()*100))+" %\nCentro",juce::dontSendNotification); };
 variacionKnob.onValueChange=[this]{ proc.variationAmount=(float)variacionKnob.getValue(); proc.applyVariation(proc.variationAmount); variacionLabel.setText(juce::String((int)std::round(variacionKnob.getValue()*100))+" %\nVariación",juce::dontSendNotification); };
 dragBtn.onClick=[this]{ auto f=proc.exportForDragAndDrop(); if(f.existsAsFile()){ juce::StringArray files; files.add(f.getFullPathName()); // JUCE drag: usar DragAndDropContainer en el editor si se necesita
  juce::DragAndDropContainer::findParentDragContainerFor(this)->startDragging(juce::var(files),this,juce::Image(),true); } };
 proc.addChangeListener(this); startTimerHz(30);
}
LaPautaTuneEditor::~LaPautaTuneEditor(){ proc.removeChangeListener(this); stopTimer(); }
void LaPautaTuneEditor::paint(juce::Graphics& g){ g.fillAll(juce::Colour(0xff1b232b)); g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(13.f)); g.drawText("Tonalidad",10,52,80,24,juce::Justification::centredLeft); g.drawText("Escala",168,52,55,24,juce::Justification::centredLeft); g.drawText("Rejilla",10,88,60,24,juce::Justification::centredLeft); g.drawText("BPM",178,88,40,24,juce::Justification::centredLeft); g.drawText("Zoom",302,88,40,24,juce::Justification::centredLeft); }
void LaPautaTuneEditor::resized(){
 auto a=getLocalBounds(); auto top=a.removeFromTop(124);
 // Fila 1
 loadBtn.setBounds(10,8,110,28); playBtn.setBounds(126,8,62,28); stopBtn.setBounds(194,8,62,28); exportBtn.setBounds(262,8,118,28);
 tonoBtn.setBounds(400,8,72,28); tiempoBtn.setBounds(478,8,82,28); corteBtn.setBounds(566,8,72,28);
 centroKnob.setBounds(getWidth()-280,4,58,58); variacionKnob.setBounds(getWidth()-200,4,58,58); titleLabel.setBounds(getWidth()-140,4,130,24); dragBtn.setBounds(getWidth()-132,32,120,60);
 centroLabel.setBounds(getWidth()-280,62,58,32); variacionLabel.setBounds(getWidth()-200,62,58,32);
 // Fila 2
 keyBox.setBounds(88,52,68,24); scaleBox.setBounds(228,52,158,24); lockBtn.setBounds(400,52,142,28); centerBtn.setBounds(548,52,82,28); resetBtn.setBounds(636,52,104,28); undoBtn.setBounds(746,52,90,28);
 // Fila 3
 gridBox.setBounds(72,88,88,24); bpmBox.setBounds(220,88,88,24); zoomSlider.setBounds(346,88,180,24); statusLabel.setBounds(540,88,getWidth()-820,24);
 roll.setBounds(0,124,getWidth(),getHeight()-124);
}
void LaPautaTuneEditor::changeListenerCallback(juce::ChangeBroadcaster*){ refreshFromProcessor(); }
void LaPautaTuneEditor::timerCallback(){ roll.setPlayheadSeconds(proc.getPlayPosSeconds()); roll.projectChanged(); statusLabel.setText(proc.getMessage(),juce::dontSendNotification); }
void LaPautaTuneEditor::refreshFromProcessor(){ keyBox.setSelectedId(proc.key+1,juce::dontSendNotification); scaleBox.setSelectedId(proc.scaleType+1,juce::dontSendNotification); repaint(); }
void LaPautaTuneEditor::setTool(PianoRoll::Tool t){ roll.setTool(t); styleBtn(tonoBtn,t==PianoRoll::Tool::pitch); styleBtn(tiempoBtn,t==PianoRoll::Tool::time); styleBtn(corteBtn,t==PianoRoll::Tool::cut); }
bool LaPautaTuneEditor::isInterestedInFileDrag(const juce::StringArray& f){ for(auto &s:f) if(s.endsWithIgnoreCase(".wav")||s.endsWithIgnoreCase(".mp3")||s.endsWithIgnoreCase(".flac")||s.endsWithIgnoreCase(".ogg")||s.endsWithIgnoreCase(".aiff")) return true; return false; }
void LaPautaTuneEditor::filesDropped(const juce::StringArray& f,int,int){ if(f.size()>0) proc.loadFile(juce::File(f[0])); }

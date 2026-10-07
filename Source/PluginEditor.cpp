#include "PluginEditor.h"
namespace {
juce::Font uiFont(float size){return pocketFont(size);}
// Written as raw UTF-8 bytes on purpose: a \u escape in a narrow literal gets
// transcoded to the compiler's execution charset (MSVC without /utf-8 turns it
// into '?'), which is exactly how the dials lost their infinity sign.
constexpr const char* kInfinity="\xe2\x88\x9e";
constexpr const char* kMinusInfinity="-\xe2\x88\x9e";
void text(juce::Graphics& g,const juce::String& s,juce::Rectangle<float> r,float size,juce::Colour c,int align=juce::Justification::centredLeft,float glow=0.f){
    g.setFont(uiFont(juce::jmax(13.2f,size)));
    if(glow>0.f){g.setColour(c.withAlpha(glow));const float o[8][2]={{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{1,1},{-1,1},{1,-1}};for(auto& d:o)g.drawText(s,r.translated(d[0],d[1]),align);}
    g.setColour(c);g.drawText(s,r,align);
}
void stroke(juce::Graphics& g,const juce::Path& p,juce::Colour c,float width){g.setColour(c);g.strokePath(p,juce::PathStrokeType(width,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));}

}
juce::Colour PocketLook::pick(juce::uint32 neon,juce::uint32 dark,juce::uint32 white) const {
    if(theme==PocketTheme::Neon)return juce::Colour(neon);
    if(theme==PocketTheme::SolidDark)return juce::Colour(dark);
    juce::ignoreUnused(white);
    // Amber: preserve the semantic brightness/alpha of the dark palette while
    // moving it onto the warm brown/orange ramp from the reference UI.
    const auto source=juce::Colour(dark?dark:neon);
    const float b=source.getPerceivedBrightness();
    const float sat=juce::jmap(b,0.f,1.f,.72f,.20f);
    const float value=juce::jlimit(.025f,1.f,b*.93f+.018f);
    return juce::Colour::fromHSV(.078f,sat,value,source.getFloatAlpha());
}
juce::Colour PocketLook::ink() const{return tokens().ink;}
juce::Colour PocketLook::muted() const{return tokens().muted;}
juce::Colour PocketLook::accent() const{return theme==PocketTheme::Amber?juce::Colour(0xffff7126):pick(0xff35d6dc,0xffe8e8e8,0xff2f74d0);}
juce::Colour PocketLook::accent2() const{return theme==PocketTheme::Amber?juce::Colour(0xffffd164):(isNeon()?juce::Colour(0xff35d6dc):accent());}
juce::Colour PocketLook::themedAccent(juce::uint32 neon) const {return isAmber()?(juce::Colour(neon).getHue()>.3f?juce::Colour(0xffffd164):juce::Colour(0xffff7126)):juce::Colour(neon);}
juce::Font PocketLook::getTextButtonFont(juce::TextButton&,int){return uiFont(15);}
void PocketLook::drawButtonBackground(juce::Graphics& g,juce::Button& button,const juce::Colour&,bool hover,bool down){
    if(button.getButtonText()=="expand")return;
    const auto t=tokens();auto r=button.getLocalBounds().toFloat().reduced(1);
    if(button.getButtonText()=="freeze")r=r.withSizeKeepingCentre(r.getWidth()*20.2746f/32.f,r.getHeight()*20.2746f/32.f);
    g.setColour(hover?t.raised.brighter(.12f):t.raised);g.fillRoundedRectangle(r,2.5f);
    g.setColour(down?t.ink:t.out.withAlpha(.75f));g.drawRoundedRectangle(r,2.5f,1.2f);
}
void PocketLook::drawButtonText(juce::Graphics& g,juce::TextButton& b,bool,bool){
    auto r=b.getLocalBounds().toFloat();auto name=b.getButtonText();
    if(name=="freeze")r=r.withSizeKeepingCentre(r.getWidth()*20.2746f/32.f,r.getHeight()*20.2746f/32.f);
    const auto controlInk=isDark()?tokens().out:ink();
    auto c=r.getCentre();float s=juce::jmin(r.getWidth(),r.getHeight())/40;juce::Path p;
    if(name=="expand"){
        const float direction=b.getToggleState()?-1.f:1.f;
        p.startNewSubPath(c.x-10*s,c.y-3*s*direction);p.lineTo(c.x,c.y+3*s*direction);p.lineTo(c.x+10*s,c.y-3*s*direction);juce::Path outline;juce::PathStrokeType(1.7f*s).createStrokedPath(outline,p);
        juce::DropShadow(ink().withAlpha(.30f),7,{0,0}).drawForPath(g,outline);
        juce::DropShadow(ink().withAlpha(.55f),3,{0,0}).drawForPath(g,outline);
        stroke(g,p,ink().brighter(.2f),1.7f*s);return;
    }
    if(name=="power"){p.addCentredArc(0,1,8,8,0,.65f,juce::MathConstants<float>::twoPi-.65f,true);p.startNewSubPath(0,-10);p.lineTo(0,-1);p.applyTransform(juce::AffineTransform::scale(s).translated(c.x,c.y));stroke(g,p,controlInk,1.7f*s);return;}

    if(name=="freeze"){
        // small snowflake: three crossed arms with barbs, lit up while frozen
        for(int arm=0;arm<3;++arm){
            const float a=juce::MathConstants<float>::halfPi+float(arm)*juce::MathConstants<float>::pi/3.f;
            const float dx=std::cos(a),dy=std::sin(a);
            p.startNewSubPath(-dx*10,-dy*10);p.lineTo(dx*10,dy*10);
            for(float sign:{-1.f,1.f})for(float at:{5.5f,9.f}){
                const float bx=dx*at*sign,by=dy*at*sign;
                for(float spread:{.62f,-.62f}){
                    const float ax=std::cos(a+spread),ay=std::sin(a+spread);
                    p.startNewSubPath(bx,by);p.lineTo(bx+ax*3.2f*sign,by+ay*3.2f*sign);
                }
            }
        }
        p.applyTransform(juce::AffineTransform::scale(s).translated(c.x,c.y));
        stroke(g,p,(b.getToggleState()?accent():controlInk).withAlpha(b.isEnabled()?1.f:.35f),1.35f*s);return;
    }
    if(name!="settings"){g.setFont(pocketFont(13.f,false,true));g.setColour(b.getToggleState()?tokens().out:tokens().muted);g.drawText(name,r,juce::Justification::centred);return;}
    constexpr int teeth=10;for(int i=0;i<teeth*4;++i){float a=float(i)*juce::MathConstants<float>::twoPi/float(teeth*4)-juce::MathConstants<float>::halfPi;float radius=(i%4==1||i%4==2)?10.f:7.7f;auto pt=juce::Point<float>(std::cos(a)*radius,std::sin(a)*radius);if(i==0)p.startNewSubPath(pt);else p.lineTo(pt);}p.closeSubPath();p.applyTransform(juce::AffineTransform::scale(s).translated(c.x,c.y));stroke(g,p,controlInk,1.65f*s);g.setColour(controlInk);g.drawEllipse(c.x-3.2f*s,c.y-3.2f*s,6.4f*s,6.4f*s,1.65f*s);
}
void PocketLook::drawLinearSlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float minPos,float maxPos,juce::Slider::SliderStyle style,juce::Slider& slider){
    auto t=tokens();
    const float scale=float(slider.getWidth())/670.f;
    const float cy=float(y)+float(h)*.5f,left=style==juce::Slider::TwoValueHorizontal?minPos:float(x),right=style==juce::Slider::TwoValueHorizontal?maxPos:pos;
    const auto light=theme==PocketTheme::SolidDark?juce::Colour(0xffe2f5f8):(slider.getName()=="key"?t.key:t.out);
    g.setColour(t.glass.darker(.6f));g.fillRoundedRectangle(float(x),cy-4*scale,float(w),8*scale,3*scale);
    g.setColour(t.border.withAlpha(.6f));g.drawLine(float(x),cy+4*scale,float(x+w),cy+4*scale,.6f*scale);
    g.setColour(light.withAlpha(.18f));g.fillRoundedRectangle(left,cy-6*scale,juce::jmax(.1f,right-left),12*scale,4*scale);
    g.setColour(light);g.fillRect(left,cy-2.5f*scale,juce::jmax(.1f,right-left),5*scale);
    auto thumb=[&](float px){auto r=juce::Rectangle<float>(13*scale,13*scale).withCentre({px,cy});
        g.setColour(juce::Colours::black.withAlpha(.45f));g.fillRoundedRectangle(r.translated(scale,2*scale),2*scale);
        g.setGradientFill(juce::ColourGradient(t.out.interpolatedWith(t.raised,.6f),r.getX(),r.getY(),t.glass,r.getRight(),r.getBottom(),false));g.fillRoundedRectangle(r,2*scale);
        g.setColour(t.ink.withAlpha(.25f));g.drawRoundedRectangle(r,2*scale,.65f*scale);};
    if(style==juce::Slider::TwoValueHorizontal){thumb(minPos);thumb(maxPos);}else thumb(pos);
}
ModernDial::ModernDial(PocketLook& l,juce::String t,juce::String sub,juce::String u,juce::uint32 a,bool inf,bool infMin,bool compactDial,juce::String infLabel):look(l),title(t),subtitle(sub),unit(u),infinity(inf),compact(compactDial){juce::ignoreUnused(a,infMin,infLabel);setSliderStyle(juce::Slider::RotaryVerticalDrag);setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);setName(t);setWantsKeyboardFocus(true);}
void ModernDial::paint(juce::Graphics& g){
    const auto t=look.tokens();const float designWidth=compact?132.f:240.f,designHeight=compact?135.f:250.f;
    const float factor=float(getWidth())/designWidth;
    const float scale=g.getInternalContext().getPhysicalPixelScaleFactor();
    const int pw=juce::jmax(1,juce::roundToInt(getWidth()*scale)),ph=juce::jmax(1,juce::roundToInt(getHeight()*scale));
    const juce::Point<float> centre=compact?juce::Point<float>{66.26f,75.40f}:juce::Point<float>{119.27f,135.27f};
    const float radius=compact?49.68f:97.5f,faceRadius=compact?31.34f:60.f;
    if(!body.isValid()||body.getWidth()!=pw||body.getHeight()!=ph||bodyTheme!=look.theme||std::abs(bodyScale-scale)>.001f){
        body=juce::Image(juce::Image::ARGB,pw,ph,true,juce::SoftwareImageType());bodyTheme=look.theme;bodyScale=scale;
        ringValue=std::numeric_limits<double>::quiet_NaN();juce::Graphics bg(body);bg.addTransform(juce::AffineTransform::scale(scale*factor));
        if(look.theme==PocketTheme::SolidDark){
            static const auto large=juce::ImageFileFormat::loadFrom(BinaryData::DialLarge_png,BinaryData::DialLarge_pngSize);
            static const auto small=juce::ImageFileFormat::loadFrom(BinaryData::DialSmall_png,BinaryData::DialSmall_pngSize);
            bg.setImageResamplingQuality(juce::Graphics::highResamplingQuality);bg.drawImage(compact?small:large,{0,0,designWidth,designHeight},juce::RectanglePlacement::stretchToFit);
            // Cool, deeper material grade is cached with the SVG-derived body.
            bg.setGradientFill(juce::ColourGradient(juce::Colour(0xff142837).withAlpha(.16f),centre.x-radius,centre.y-radius,juce::Colour(0xff020b13).withAlpha(.40f),centre.x+radius,centre.y+radius,false));
            bg.fillEllipse(centre.x-radius,centre.y-radius,2*radius,2*radius);
        }else{
            auto rim=juce::Rectangle<float>(2*radius,2*radius).withCentre(centre);
            bg.setGradientFill(juce::ColourGradient(t.raised.brighter(.2f),centre.x-radius,centre.y-radius,t.glass.darker(.15f),centre.x+radius,centre.y+radius,false));bg.fillEllipse(rim);
            bg.setColour(t.glass.darker(.5f));bg.drawEllipse(rim,compact?5.f:6.f);
            auto face=juce::Rectangle<float>(2*faceRadius,2*faceRadius).withCentre(centre);
            bg.setGradientFill(juce::ColourGradient(t.glass,centre.x-faceRadius,centre.y-faceRadius,t.raised,centre.x+faceRadius,centre.y+faceRadius,false));bg.fillEllipse(face);
            bg.setColour(t.border);bg.drawEllipse(face,1.5f);
        }
        // Preserve the original curved-heading construction with new labels.
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText(pocketFont(compact?13.f:19.f,false,true),title,0,0);
        const float width=glyphs.getBoundingBox(0,-1,true).getWidth(),r=radius+(compact?13.f:18.f);
        bg.setColour(t.ink);
        for(int i=0;i<glyphs.getNumGlyphs();++i){const auto& glyph=glyphs.getGlyph(i);juce::Path path;glyph.createPath(path);
            const float x=glyph.getBounds().getCentreX(),angle=(x-width*.5f)/r;
            bg.fillPath(path,juce::AffineTransform::translation(-x,0).rotated(angle).translated(centre.x+std::sin(angle)*r,centre.y-std::cos(angle)*r));}
    }
    g.setOpacity(isEnabled()?1.f:.35f);g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);g.drawImageTransformed(body,juce::AffineTransform::scale(1.f/bodyScale));
    // Cache the blurred ring independently; audio-driven metering never rebuilds it.
    const float proportion=float(valueToProportionOfLength(getValue()));
    if(!ringImage.isValid()||ringValue!=double(proportion)||ringImage.getWidth()!=pw||ringImage.getHeight()!=ph){
        ringValue=double(proportion);ringImage=juce::Image(juce::Image::ARGB,pw,ph,true,juce::SoftwareImageType());juce::Graphics rg(ringImage);rg.addTransform(juce::AffineTransform::scale(scale*factor));
        const float start=juce::MathConstants<float>::pi,end=start+juce::MathConstants<float>::twoPi*proportion;
        const auto colour=look.theme==PocketTheme::SolidDark?juce::Colour(0xfff5f8ff):(title=="Influence"||title=="Output"?t.out:t.neutral);
        juce::Path arc;
        if(proportion>=.99999f)arc.addEllipse(centre.x-radius,centre.y-radius,2*radius,2*radius);
        else if(proportion>0)arc.addCentredArc(centre.x,centre.y,radius,radius,0,start,end,true);
        // Rasterised once per value/size/theme, no idle glow animation.
        if(!arc.isEmpty()){
            juce::Path outline;juce::PathStrokeType(compact?4.2f:4.6f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded).createStrokedPath(outline,arc);
            juce::DropShadow(colour.withAlpha(.5f),14,{0,0}).drawForPath(rg,outline);
            juce::DropShadow(colour.withAlpha(.9f),5,{0,0}).drawForPath(rg,outline);
            stroke(rg,arc,colour,compact?4.2f:4.6f);
        }
        const float inner=faceRadius+8.f,outer=inner+(compact?9.f:16.f);
        rg.setColour(t.ink.withAlpha(.75f));rg.drawLine(centre.x+inner*std::sin(end),centre.y-inner*std::cos(end),centre.x+outer*std::sin(end),centre.y-outer*std::cos(end),1.2f);
    }
    g.drawImageTransformed(ringImage,juce::AffineTransform::scale(1.f/bodyScale));
    juce::Graphics::ScopedSaveState save(g);g.addTransform(juce::AffineTransform::scale(factor));
    if(emphasis>.001f){g.setColour(t.ink.withAlpha(emphasis*.10f));g.drawEllipse(centre.x-faceRadius,centre.y-faceRadius,2*faceRadius,2*faceRadius,1.f);}
    const auto value=displayedValue();
    g.setFont(pocketFont(valueTextHeight(value)/factor));g.setColour(t.ink.withMultipliedAlpha(isEnabled()?1.f:.35f));
    g.drawText(value,juce::Rectangle<float>{centre.x-faceRadius,centre.y-(subtitle.isEmpty()&&unit=="%"?12.5f:(compact?15.f:19.f)),2*faceRadius,compact?25.f:35.f},juce::Justification::centred);
    const auto detail=unit=="style"?juce::String(juce::roundToInt(getValue())*50)+"%":((unit=="dB"||unit=="ms")?unit:subtitle);
    g.setFont(pocketFont(compact?11.f:13.f));g.setColour(t.muted.withMultipliedAlpha(isEnabled()?1.f:.35f));g.drawText(detail,juce::Rectangle<float>{centre.x-faceRadius,centre.y+(compact?8.f:17.f),2*faceRadius,20.f},juce::Justification::centred);
}

void PocketLook::drawComboBox(juce::Graphics& g,int width,int height,bool,int,int,int,int,juce::ComboBox& box){
 const auto t=tokens();const auto bounds=juce::Rectangle<float>(0,0,float(width),float(height)).reduced(.5f);
 g.setColour(t.glass);g.fillRoundedRectangle(bounds,5.f);g.setColour(t.border);g.drawRoundedRectangle(bounds,5.f,1.f);
 juce::Path arrow;const float x=float(width)-14.f,y=float(height)*.5f;arrow.startNewSubPath(x-4.f,y-2.f);arrow.lineTo(x,y+2.f);arrow.lineTo(x+4.f,y-2.f);g.setColour(t.ink.withAlpha(box.isEnabled()?1.f:.35f));g.strokePath(arrow,juce::PathStrokeType(1.5f));
}

void ModernDial::mouseDown(const juce::MouseEvent& event){
 if(!event.mods.isPopupMenu()){juce::Slider::mouseDown(event);animate(1);return;}
 if(unit=="style"){juce::PopupMenu menu;const char* names[]{"0% - Clean","50% - Punchy","100% - Analog"};for(int i=0;i<3;++i)menu.addItem(i+1,names[i],true,juce::roundToInt(getValue())==i);auto safe=juce::Component::SafePointer<ModernDial>(this);menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),[safe](int id){if(safe&&id>0)safe->setValue(id-1,juce::sendNotificationSync);});return;}
 auto* dialog=new juce::AlertWindow(title,"Enter a value ("+unit+")",juce::MessageBoxIconType::NoIcon);
 dialog->addTextEditor("value",juce::String(getValue(),2),title);dialog->addButton("Apply",1,juce::KeyPress(juce::KeyPress::returnKey));dialog->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
 auto safe=juce::Component::SafePointer<ModernDial>(this);
 dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe,dialog](int result){if(result==1&&safe){const double value=dialog->getTextEditorContents("value").getDoubleValue();safe->setValue(juce::jlimit(safe->getMinimum(),safe->getMaximum(),value),juce::sendNotificationSync);}}),true);
}
void ClipMeter::paint(juce::Graphics& g){
 const auto t=look.tokens();const float scale=float(getWidth())/70.f;
 const float top=54.f*scale,bottom=float(getHeight())-8.f*scale,height=bottom-top;
 const juce::Rectangle<float> bar{34.f*scale,top,26.f*scale,height};
 g.setColour(t.muted);g.setFont(pocketFont(10.f*scale));g.drawText(name,juce::Rectangle<int>{0,0,getWidth(),juce::roundToInt(19.f*scale)},juce::Justification::centred);
 const auto value=gr?juce::String(held,2):(held>1.e-9f?juce::String(clip::gainDb(held),2):juce::String("-inf"));
 g.setColour(t.ink);g.setFont(pocketFont(12.f*scale));g.drawText(value+" dB",juce::Rectangle<int>{0,juce::roundToInt(20.f*scale),getWidth(),juce::roundToInt(23.f*scale)},juce::Justification::centred);
 const float db=gr?juce::jlimit(0.f,30.f,level):juce::jlimit(-30.f,0.f,float(clip::gainDb(level)));
 g.setColour(t.glass.darker(.3f));g.fillRect(bar);g.setColour(t.border);g.drawRect(bar,.6f*scale);
 const int segments=100;const float step=height/segments;
 for(int i=0;i<segments;++i){const float at=30.f*float(i)/segments;const bool active=gr?at<db:at>=-db;
  if(active){g.setColour(gr?t.key:(at<3.f?juce::Colour(0xffe78555):t.out));g.fillRect(bar.getX()+scale,top+i*step+scale*.25f,bar.getWidth()-2.f*scale,juce::jmax(.5f,step-.7f*scale));}}
 for(int i=0;i<=10;++i){const float y=top+height*float(i)/10.f;g.setColour(t.muted);g.setFont(pocketFont(9.f*scale));g.drawText(i==0?"0":juce::String(-3*i),juce::Rectangle<int>{0,juce::roundToInt(y-7.f*scale),juce::roundToInt(28.f*scale),juce::roundToInt(14.f*scale)},juce::Justification::centredRight);}
}
ClipPocketAudioProcessorEditor::ClipPocketAudioProcessorEditor(ClipPocketAudioProcessor& p):AudioProcessorEditor(&p),audioProcessor(p){
 juce::PropertiesFile::Options o;o.applicationName="ClipPocket";o.filenameSuffix="settings";o.folderName="RainlineMusic";o.osxLibrarySubFolder="Application Support";preferences=std::make_unique<juce::PropertiesFile>(o);
 const auto saved=preferences->getValue("clipPocket.theme","solidDark");setTheme(saved=="neon"?PocketTheme::Neon:(saved=="amber"?PocketTheme::Amber:PocketTheme::SolidDark),false);
 tooltip.setMillisecondsBeforeTipAppears(preferences->getBoolValue("clipPocket.tooltips",true)?800:10000000);
 setLookAndFeel(&look);setOpaque(true);setResizable(true,true);
 for(auto* component:std::initializer_list<juce::Component*>{&input,&ceiling,&output,&bass,&outMeter,&grMeter,&settingsButton,&bypassButton,&style,&knee})addAndMakeVisible(component);
 auto attach=[&](ModernDial& dial,const char* id,float defaultValue,const char* hint){attachments.push_back(std::make_unique<Attachment>(p.parameters,id,dial));dial.setDoubleClickReturnValue(true,defaultValue);dial.setTooltip(hint);};
 attach(input,"in",0,"Input drive. Double-click resets; right-click enters a value.");attach(ceiling,"ceiling",0,"Clipping threshold. Reconstructed samples are bounded after Output; no true-peak limiting.");
 attach(output,"output",0,"Output trim after clipping.");attach(bass,"bass",0,"Smoothly limit the band below 100 Hz instead of clipping it. Prioritises kick and sub-bass; may reduce their level.");
 attach(style,"mode",0,"Three fixed styles: 0% Clean, 50% Punchy, 100% Analog.");attach(knee,"knee",0,"Clipping knee: 0% hard, 100% soft.");
 bypassButton.setClickingTogglesState(true);
 bypassAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters,"bypass",bypassButton);
 outMeter.onReset=[this]{audioProcessor.meterOut.exchange(0);};grMeter.onReset=[this]{audioProcessor.meterGR.exchange(0);};
 outMeter.setTooltip("Output sample peak. Click the value to reset.");grMeter.setTooltip("Maximum gain reduction. Click the value to reset.");
 settingsButton.onClick=[this]{showSettingsMenu();};
 getConstrainer()->setSizeLimits(600,juce::roundToInt(designHeight()*.75f),1400,juce::roundToInt(designHeight()*1.75f));getConstrainer()->setFixedAspectRatio(800./designHeight());
 const int width=juce::jlimit(600,1400,preferences->getIntValue("clipPocket.width",800));setSize(width,juce::roundToInt(width*designHeight()/800.f));resized();startTimerHz(30);timerCallback();
}
ClipPocketAudioProcessorEditor::~ClipPocketAudioProcessorEditor(){stopTimer();
#if CLIP_ENABLE_OPENGL
 if(openGL)openGL->detach();
 openGL.reset();
#endif
 saveSize();setLookAndFeel(nullptr);
}
juce::Rectangle<int> ClipPocketAudioProcessorEditor::scaled(float x,float y,float w,float h) const {const float s=float(getWidth())/800.f;return juce::Rectangle<float>(x*s,y*s,w*s,h*s).toNearestInt();}
void ClipPocketAudioProcessorEditor::resized(){
 input.setBounds(scaled(32,76,240,250));ceiling.setBounds(scaled(326,76,240,250));
 style.setBounds(scaled(30,352,132,135));knee.setBounds(scaled(180,352,132,135));bass.setBounds(scaled(330,352,132,135));output.setBounds(scaled(480,352,132,135));
 settingsButton.setBounds(scaled(696,12,32,32));bypassButton.setBounds(scaled(744,12,32,32));
 outMeter.setBounds(scaled(640,76,70,420));grMeter.setBounds(scaled(724,76,70,420));
 blurArea=scaled(16,60,768,designHeight()-72);chromeValid=false;blurredSnapshot={};audioProcessor.editorWidth=getWidth();
}
void ClipPocketAudioProcessorEditor::saveSize(){preferences->setValue("clipPocket.width",getWidth());preferences->saveIfNeeded();}
void ClipPocketAudioProcessorEditor::setTheme(PocketTheme theme,bool persist){look.theme=theme;chromeValid=false;blurredSnapshot={};
 if(persist&&preferences){preferences->setValue("clipPocket.theme",theme==PocketTheme::Neon?"neon":theme==PocketTheme::Amber?"amber":"solidDark");preferences->saveIfNeeded();}repaint();for(auto* child:getChildren())child->repaint();}
void ClipPocketAudioProcessorEditor::drawChrome(juce::Graphics& g){
 const float pixel=juce::jlimit(.75f,4.f,g.getInternalContext().getPhysicalPixelScaleFactor());const int w=juce::jmax(1,juce::roundToInt(getWidth()*pixel)),h=juce::jmax(1,juce::roundToInt(getHeight()*pixel));
 if(!chromeValid||chrome.getWidth()!=w||chrome.getHeight()!=h||std::abs(chromeScale-pixel)>.001f){
  chrome=juce::Image(juce::Image::ARGB,w,h,true,juce::SoftwareImageType());chromeScale=pixel;++chromeBuilds;juce::Graphics cg(chrome);cg.addTransform(juce::AffineTransform::scale(pixel*float(getWidth())/800.f));const auto t=look.tokens();
  cg.setGradientFill(juce::ColourGradient(t.chassis,400,240,t.chassis.darker(.2f),0,502,true));cg.fillRect(0.f,0.f,800.f,designHeight());
  juce::Random noise(0xD0C);for(int i=0;i<6000;++i){cg.setColour((i%2?juce::Colours::white:juce::Colours::black).withAlpha(.012f));cg.fillRect(float(noise.nextInt(800)),float(noise.nextInt(int(designHeight()))),1.f,1.f);}
  text(cg,"CLIP POCKET",{22,10,200,34},21,t.brand,juce::Justification::centredLeft);
  cg.setColour(juce::Colours::black.withAlpha(.5f));cg.fillRect(0.f,54.f,800.f,3.f);cg.setColour(t.ink.withAlpha(.05f));cg.drawLine(0,57,800,57,.7f);
  cg.setGradientFill(juce::ColourGradient(t.glass.darker(.2f),630,76,t.glass.darker(.4f),800,496,false));cg.fillRoundedRectangle(628.f,64.f,166.f,444.f,7.f);
  chromeValid=true;
 }
 g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);g.drawImage(chrome,getLocalBounds().toFloat());
}
void ClipPocketAudioProcessorEditor::paint(juce::Graphics& g){drawChrome(g);}
void ClipPocketAudioProcessorEditor::captureBlurSnapshot(){
 if(capturingBlur||blurArea.isEmpty())return;
 capturingBlur=true;auto source=createComponentSnapshot(blurArea,true,1.f);capturingBlur=false;if(!source.isValid())return;
 const int w=juce::jmax(16,source.getWidth()/2),h=juce::jmax(16,source.getHeight()/2);juce::Image small(juce::Image::ARGB,w,h,true,juce::SoftwareImageType());
 {juce::Graphics g(small);g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);g.drawImage(source,small.getBounds().toFloat());}
 juce::Image horizontal(juce::Image::ARGB,w,h,true,juce::SoftwareImageType()),soft(juce::Image::ARGB,w,h,true,juce::SoftwareImageType());
 const int radius=juce::jmax(1,juce::roundToInt(float(getWidth())/800.f*3.f));for(int pass=0;pass<3;++pass){PocketSoftwareGlow::boxBlur(pass==0?small:soft,horizontal,true,radius);PocketSoftwareGlow::boxBlur(horizontal,soft,false,radius);}blurredSnapshot=soft;
}
void ClipPocketAudioProcessorEditor::paintOverChildren(juce::Graphics& g){if(capturingBlur||!bypassTarget||!blurredSnapshot.isValid())return;
 g.drawImage(blurredSnapshot,blurArea.toFloat());g.setColour(juce::Colours::black.withAlpha(.18f));g.fillRoundedRectangle(blurArea.toFloat(),12.f);
 text(g,"BYPASSED",blurArea.toFloat(),float(getWidth())/18.f,look.tokens().ink,juce::Justification::centred);
}
void ClipPocketAudioProcessorEditor::timerCallback(){
 audioProcessor.meterIn.exchange(0);const auto freshOut=audioProcessor.meterOut.exchange(0),freshGR=audioProcessor.meterGR.exchange(0);
 meterOutput=juce::jmax(freshOut,meterOutput*.86f);meterReduction=juce::jmax(freshGR,meterReduction*.86f);
 const bool target=audioProcessor.parameters.getRawParameterValue("bypass")->load()>.5f||audioProcessor.displayBypass.load();
 if(target!=bypassTarget){bypassTarget=target;blurredSnapshot={};repaint();}
 bass.setEnabled(!target);style.setEnabled(!target);knee.setEnabled(!target);
 if(target){if(!blurredSnapshot.isValid()){captureBlurSnapshot();repaint();}return;}
 outMeter.setLevel(meterOutput,freshOut);grMeter.setLevel(meterReduction,freshGR);
}
void ClipPocketAudioProcessorEditor::showSettingsMenu(){
 juce::PopupMenu root,theme,quality;theme.addItem(201,"Solid Dark",true,look.theme==PocketTheme::SolidDark);theme.addItem(202,"Neon",true,look.theme==PocketTheme::Neon);theme.addItem(203,"Amber",true,look.theme==PocketTheme::Amber);root.addSubMenu("Theme",theme);
 const auto q=juce::roundToInt(audioProcessor.parameters.getRawParameterValue("quality")->load());const char* names[]{"Eco 2x","Studio 4x","Master 8x","Ultra 16x","Extreme 32x","Offline 64x (CPU heavy)"};for(int i=0;i<6;++i)quality.addItem(300+i,names[i],true,q==i);root.addSubMenu("Quality",quality);
 root.addSeparator();
 root.addItem(505,"Maximum 64x on offline render",true,audioProcessor.parameters.getRawParameterValue("renderHQ")->load()>.5f);
 root.addItem(503,"Tooltips",true,preferences->getBoolValue("clipPocket.tooltips",true));
#if CLIP_ENABLE_OPENGL && ! JUCE_WINDOWS
 root.addItem(504,"OpenGL (experimental)",true,openGL!=nullptr);
#endif
 juce::StringArray rendererNames;
#if JUCE_WINDOWS
 if(auto* peer=getPeer()){rendererNames=peer->getAvailableRenderingEngines();juce::PopupMenu render;for(int i=0;i<rendererNames.size();++i)render.addItem(600+i,rendererNames[i],true,i==peer->getCurrentRenderingEngine());root.addSubMenu("Windows renderer",render);}
#endif
 root.addSeparator();root.addItem(900,"Latency: "+juce::String(audioProcessor.getLatencySamples())+" samples",false);root.addItem(901,"Clip Pocket 0.4.0",false);
 auto safe=juce::Component::SafePointer<ClipPocketAudioProcessorEditor>(this);root.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(settingsButton),[safe,rendererNames](int id){if(!safe||!id)return;
 if(id>=201&&id<=203)safe->setTheme(id==201?PocketTheme::SolidDark:id==202?PocketTheme::Neon:PocketTheme::Amber);
 else if(id>=300&&id<306)safe->audioProcessor.setParameterValue("quality",float(id-300));
 else if(id==505)safe->audioProcessor.setParameterValue("renderHQ",safe->audioProcessor.parameters.getRawParameterValue("renderHQ")->load()>.5f?0.f:1.f);
 else if(id==503){const bool enabled=!safe->preferences->getBoolValue("clipPocket.tooltips",true);safe->preferences->setValue("clipPocket.tooltips",enabled);safe->tooltip.setMillisecondsBeforeTipAppears(enabled?800:10000000);}
#if CLIP_ENABLE_OPENGL && ! JUCE_WINDOWS
 else if(id==504)safe->setOpenGL(safe->openGL==nullptr);
#endif
 else if(id>=600&&id<600+rendererNames.size())safe->setWindowsRenderer(rendererNames[id-600]);});
}
void ClipPocketAudioProcessorEditor::setWindowsRenderer(const juce::String& name,bool persist){if(auto* peer=getPeer()){const int index=peer->getAvailableRenderingEngines().indexOf(name);if(index>=0){peer->setCurrentRenderingEngine(index);chromeValid=false;repaint();if(persist)preferences->setValue("clipPocket.windowsRenderer",name);}}}
void ClipPocketAudioProcessorEditor::parentHierarchyChanged(){
#if JUCE_WINDOWS
 if(preferences->containsKey("clipPocket.windowsRenderer"))setWindowsRenderer(preferences->getValue("clipPocket.windowsRenderer"),false);
#endif
#if CLIP_ENABLE_OPENGL && ! JUCE_WINDOWS
 if(getPeer()&&preferences->getBoolValue("clipPocket.opengl",false))setOpenGL(true,false);
#endif
}
#if CLIP_ENABLE_OPENGL
void ClipPocketAudioProcessorEditor::setOpenGL(bool enabled,bool persist){
#if JUCE_WINDOWS
 enabled=false;
#endif
 if(openGL){openGL->detach();openGL.reset();}if(enabled&&getPeer()){openGL=std::make_unique<juce::OpenGLContext>();openGL->setComponentPaintingEnabled(true);openGL->setContinuousRepainting(false);openGL->attachTo(*this);}if(persist)preferences->setValue("clipPocket.opengl",enabled);chromeValid=false;repaint();}
#endif


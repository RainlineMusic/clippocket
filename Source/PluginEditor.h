#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UIStyle.h"
#include "SoftwareGlow.h"
class PocketLook final:public juce::LookAndFeel_V4 {
public:
    PocketTheme theme=PocketTheme::SolidDark;
    PocketTokens tokens() const{return PocketTokens::forTheme(theme); }
    bool isDark() const{return true;}
    bool isNeon() const{return theme==PocketTheme::Neon;}
    bool isAmber() const{return theme==PocketTheme::Amber;}
    bool hasGlow() const{return true;}
    juce::Colour pick(juce::uint32 neon,juce::uint32 dark,juce::uint32 white) const;
    juce::Colour ink() const;
    juce::Colour muted() const;
    juce::Colour accent() const;
    juce::Colour accent2() const;
    juce::Colour themedAccent(juce::uint32 neon) const;
    juce::Font getTextButtonFont(juce::TextButton&,int) override;
    void drawButtonBackground(juce::Graphics&,juce::Button&,const juce::Colour&,bool,bool) override;
    void drawButtonText(juce::Graphics&,juce::TextButton&,bool,bool) override;
    void drawComboBox(juce::Graphics&,int,int,bool,int,int,int,int,juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox& box) override {return pocketFont(float(box.getHeight())*.48f);}
    void drawLinearSlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider::SliderStyle,juce::Slider&) override;
};

class ModernDial final:public juce::Slider,private juce::Timer {
public:
    // trailing infLabel overrides the "infinity" display text at full deflection
    // (e.g. "AUTO"); empty means keep the default infinity glyph.
    ModernDial(PocketLook&,juce::String,juce::String,juce::String,juce::uint32,bool=false,bool=false,bool=false,juce::String={});
    ~ModernDial() override {stopTimer();}
    bool isAutoValue() const {return infinity&&getValue()>=getMaximum();}
    float valueTextHeight(const juce::String& value) const {
        const float scale=float(getWidth())/(compact?132.f:240.f);
        const float preferred=(compact?14.f:22.f)*scale;
        const float diameter=(compact?60.f:112.f)*scale;
        const float width=juce::GlyphArrangement::getStringWidth(pocketFont(compact?14.f:22.f,true),value)*scale;
        return juce::jlimit(6.f*scale,preferred,preferred*diameter/juce::jmax(1.f,width+1.f));
    }
    void mouseEnter(const juce::MouseEvent& e) override {juce::Slider::mouseEnter(e);animate(.65f);}
    void mouseExit(const juce::MouseEvent& e) override {juce::Slider::mouseExit(e);animate(0);}
    void mouseUp(const juce::MouseEvent& e) override {juce::Slider::mouseUp(e);animate(isMouseOver()?.65f:0);}
    void paint(juce::Graphics&) override;
    juce::String displayedValue() const {
        if(unit=="dB")return juce::String(std::abs(getValue())<.005?0.:getValue(),2);
        return juce::String(getValue(),unit=="ms"?1:0)+(unit=="%"?"%":"");
    }
    void mouseDown(const juce::MouseEvent&) override;
private:
    PocketLook& look;
    juce::String title,subtitle,unit;
    bool infinity,compact;
    juce::Image body,ringImage;
    std::unique_ptr<juce::Drawable> heading;
    double ringValue=std::numeric_limits<double>::quiet_NaN();
    PocketTheme bodyTheme=PocketTheme::Neon;
    float bodyScale=0,emphasis=0,targetEmphasis=0;
    void animate(float target){targetEmphasis=target;startTimerHz(60);}
    void timerCallback() override {emphasis+=(targetEmphasis-emphasis)*.3f;if(std::abs(targetEmphasis-emphasis)<.01f){emphasis=targetEmphasis;stopTimer();}repaint();}
};


class ClipMeter final : public juce::Component {
public:
 ClipMeter(PocketLook& l,juce::String label,bool reduction=false):look(l),name(std::move(label)),gr(reduction){}
 void setLevel(float next){level=next;repaint();}
 void paint(juce::Graphics&) override;
private:PocketLook& look;juce::String name;bool gr=false;float level=0.f;
};
class ClipPocketAudioProcessorEditor final : public juce::AudioProcessorEditor,private juce::Timer {
public:
 explicit ClipPocketAudioProcessorEditor(ClipPocketAudioProcessor&);
 ~ClipPocketAudioProcessorEditor() override;
 void paint(juce::Graphics&) override;
 void paintOverChildren(juce::Graphics&) override;
 void resized() override;
 void parentHierarchyChanged() override;
private:
 friend struct ClipUiTestAccess;
 using Attachment=juce::AudioProcessorValueTreeState::SliderAttachment;
 ClipPocketAudioProcessor& audioProcessor;PocketLook look;
 ModernDial input{look,"IN","Input level","dB",0},ceiling{look,"Ceiling","Peak threshold","dB",0};
 ModernDial output{look,"Output","Trim","dB",0,false,false,true},bass{look,"Low Protect","","%",0,false,false,true};
 std::vector<std::unique_ptr<Attachment>> attachments;
 ClipMeter inMeter{look,"IN"},outMeter{look,"OUT"},grMeter{look,"GR",true};
 juce::TextButton settingsButton{"settings"},bypassButton{"power"};
 juce::TextButton deltaButton{"Delta"};
 juce::ComboBox modeBox;
 std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
 std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment,deltaAttachment;
 std::unique_ptr<juce::PropertiesFile> preferences;
 juce::TooltipWindow tooltip{this,800};
 juce::Image chrome,blurredSnapshot;juce::Rectangle<int> blurArea;
 float meterInput=0,meterOutput=0,meterReduction=0;
 bool bypassTarget=false,capturingBlur=false,chromeValid=false;
 float chromeScale=1.f;std::uint64_t chromeBuilds=0;
#if CLIP_ENABLE_OPENGL
 std::unique_ptr<juce::OpenGLContext> openGL;
 void setOpenGL(bool,bool persist=true);
#endif
 void timerCallback() override;
 void setTheme(PocketTheme,bool persist=true);
 void showSettingsMenu();
 void captureBlurSnapshot();
 void drawChrome(juce::Graphics&);
 void saveSize();
 void setWindowsRenderer(const juce::String&,bool persist=true);
 float designHeight() const {return 502.f;}
 juce::Rectangle<int> scaled(float,float,float,float) const;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipPocketAudioProcessorEditor)
};

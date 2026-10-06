#pragma once
#include <JuceHeader.h>
#include <BinaryData.h>
enum class PocketTheme { Neon, Amber, SolidDark, SolidWhite };
struct PocketTokens {
    juce::Colour chassis,raised,glass,ink,muted,border,major,minor,key,out,neutral,brand;
    static PocketTokens forTheme(PocketTheme theme){
        using C=juce::Colour;
        switch(theme){
        case PocketTheme::Neon:return {C(0xff10161d),C(0xff1c2530),C(0xff0b1118),C(0xffe5eaf0),C(0xffa8b2bf),C(0xff36404d),C(0xff394957),C(0xff24313e),C(0xff63ccd6),C(0xffbca1f3),C(0xffb7c0cd),C(0xffe5bb68)};

        case PocketTheme::Amber:return {C(0xff211c17),C(0xff302820),C(0xff171410),C(0xffeee4d8),C(0xffbdb0a1),C(0xff514538),C(0xff514739),C(0xff342d25),C(0xffcfa96d),C(0xffe78555),C(0xffbeb2a4),C(0xffc6ad78)};
        case PocketTheme::SolidWhite:return {C(0xffeef2f5),C(0xffdfe7ee),C(0xffd6e0e8),C(0xff2f4355),C(0xff61778a),C(0xffa4b6c4),C(0xffb2c3d0),C(0xffc7d4df),C(0xffa77736),C(0xff2f74d0),C(0xff576f84),C(0xff425d75)};
        case PocketTheme::SolidDark:default:return {C(0xff151d25),C(0xff263341),C(0xff14191f),C(0xffd9e3ee),C(0xffadb9c7),C(0xff3e4c5b),C(0xff46515b),C(0xff293642),C(0xffccb078),C(0xffa9cbee),C(0xffeef5ff),C(0xffc1d2e3)};
        }
    }
};
inline juce::Font pocketFont(float size,bool mono=false,bool medium=false){
    juce::ignoreUnused(mono);
    static auto sans=juce::Typeface::createSystemTypefaceFor(BinaryData::InterRegular_ttf,BinaryData::InterRegular_ttfSize);
    static auto strong=juce::Typeface::createSystemTypefaceFor(BinaryData::InterMedium_ttf,BinaryData::InterMedium_ttfSize);
    return juce::Font(juce::FontOptions(medium?strong:sans).withHeight(juce::jmax(11.f,size)));
}

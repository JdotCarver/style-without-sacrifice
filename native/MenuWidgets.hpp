#pragma once
#include "Reflection.hpp"
#include "MenuLayout.hpp"
#include <initializer_list>

namespace Wardrobe::UI {
struct Color {double r,g,b,a=1;};
inline constexpr Color ink{.015,.021,.025,.96},white{.83,.81,.75,1},muted{.43,.43,.38,1},gold{.62,.43,.17,1};
struct Field {
    FProperty* p{};void* base{};
    Field at(const wchar_t* name)const{
        if(!p||!p->IsA<FStructProperty>())throw std::runtime_error("Wardrobe UI requires a reflected struct");
        return {property(static_cast<FStructProperty*>(p)->GetStruct(),name),p->ContainerPtrToValuePtr<void>(base)};
    }
    void num(double n)const{number(p,base,n);}
    void obj(UObject* o)const{assignObject(p,base,o);}
};
inline Field arg(Call& c,const wchar_t* name){return {c.field(name),c.data()};}
inline void rgba(Field f,Color c){f.at(L"R").num(c.r);f.at(L"G").num(c.g);f.at(L"B").num(c.b);f.at(L"A").num(c.a);}
inline void slateColor(Field f,Color c){rgba(f.at(L"SpecifiedColor"),c);f.at(L"ColorUseRule").num(0);}
inline void vector(Field f,double x,double y){f.at(L"X").num(x);f.at(L"Y").num(y);}
inline UObject* child(UObject* parent,UObject* content){Call c(parent,L"AddChild");c.obj(L"Content",content).invoke();return c.resultObject();}
inline void fill(UObject* slot){if(slot){setNumber(slot,L"SetHorizontalAlignment",L"InHorizontalAlignment",0);setNumber(slot,L"SetVerticalAlignment",L"InVerticalAlignment",0);}}
inline void place(UObject* canvas,UObject* content,Layout::Rect r){
    auto slot=child(canvas,content);
    Call pos(slot,L"SetPosition");vector(arg(pos,L"InPosition"),r.x,r.y);pos.invoke();
    Call size(slot,L"SetSize");vector(arg(size,L"InSize"),r.w,r.h);size.invoke();
}
inline void visible(UObject* p,bool show){if(p)setNumber(p,L"SetVisibility",L"InVisibility",show?0:1);}
inline void tint(UObject* p,Color color,bool text=false){Call c(p,L"SetColorAndOpacity");if(text)slateColor(arg(c,L"InColorAndOpacity"),color);else rgba(arg(c,L"InColorAndOpacity"),color);c.invoke();}
inline void texture(UObject* image,UObject* tex){Call c(image,L"SetBrushFromTexture");c.obj(L"Texture",tex).num(L"bMatchSize",0).invoke();}
inline void brush(Field f,UObject* texture,Color color){
    f.at(L"ResourceObject").obj(texture);f.at(L"DrawAs").num(texture?3:1);f.at(L"Tiling").num(0);f.at(L"ImageType").num(0);
    slateColor(f.at(L"TintColor"),color);
    for(auto key:{L"Left",L"Top",L"Right",L"Bottom"})f.at(L"Margin").at(key).num(0);
    vector(f.at(L"ImageSize"),94,94);
}
inline void solid(UObject* image,Color color){Call c(image,L"SetBrush");brush(arg(c,L"InBrush"),nullptr,color);c.invoke();}
inline void buttonStyle(UObject* button,UObject* normal,UObject* active,bool tile){
    Call c(button,L"SetStyle");auto style=arg(c,L"InStyle");
    brush(style.at(L"Normal"),normal,tile?white:Color{0,0,0,0});
    brush(style.at(L"Hovered"),active,tile?gold:Color{.12,.10,.065,.72});
    brush(style.at(L"Pressed"),active,tile?white:Color{.21,.16,.08,.9});
    brush(style.at(L"Disabled"),normal,Color{.18,.18,.18,.45});
    for(auto key:{L"NormalForeground",L"HoveredForeground",L"PressedForeground",L"DisabledForeground"})slateColor(style.at(key),white);
    for(auto key:{L"NormalPadding",L"PressedPadding"})for(auto side:{L"Left",L"Top",L"Right",L"Bottom"})style.at(key).at(side).num(0);
    c.invoke();
}
inline void font(UObject* text,UObject* style,double size){
    Call c(text,L"SetFont");auto target=c.field(L"InFontInfo");auto source=property(style,L"Font");
    if(!source)source=property(text,L"Font");auto owner=property(style,L"Font")?style:text;
    if(!source||!target||!source->IsA<FStructProperty>()||!target->IsA<FStructProperty>()||static_cast<FStructProperty*>(source)->GetStruct()!=static_cast<FStructProperty*>(target)->GetStruct())throw std::runtime_error("Wardrobe font layout unavailable");
    target->CopyCompleteValue(c.value(L"InFontInfo"),source->ContainerPtrToValuePtr<void>(owner));arg(c,L"InFontInfo").at(L"Size").num(size);c.invoke();
}
inline void wrap(UObject* text,double width){
    // Fixed wrapping avoids desired-size feedback inside the page's ScaleBox.
    setNumber(text,L"SetAutoWrapText",L"InAutoTextWrap",0);number(property(text,L"WrapTextAt"),text,width);
}
}

#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace Wardrobe {
namespace Pad {
inline constexpr uint16_t Up=1,Down=2,Left=4,Right=8,Menu=16,PageUp=32,LStick=64,RStick=128,PageDown=256,A=4096,B=8192,X=16384,Y=32768;
}
struct ControllerSample {uint16_t buttons{},edges{};int x{},y{};bool left{},right{},leftEdge{},rightEdge{},cancelled{};};
// Owned engine events only. No borrowed FKey, FInputEvent, frame or UObject is
// retained. Repeats do not manufacture edges; a press/release between UI ticks
// still delivers one edge. Suspending consumes pending actions and held inputs.
struct ControllerState {
    uint16_t held{},blocked{},edges{};int x{},y{};bool left{},right{},leftEdge{},rightEdge{},enabled{},blockStick{},blockLeft{},blockRight{};
    void enable(bool value){
        if(value==enabled)return;enabled=value;edges=0;leftEdge=rightEdge=false;
        blocked=held;blockStick=std::max(std::abs(x),std::abs(y))>=9000;blockLeft=left;blockRight=right;
    }
    void suspend(){enable(false);edges=0;leftEdge=rightEdge=false;blocked=held;}
    void clear(){*this={};}
    void button(uint16_t bit,bool down,bool repeat){
        if(!down){held&=~bit;blocked&=~bit;return;}
        if(enabled&&!repeat&&!(held&bit)&&!(blocked&bit))edges|=bit;
        if(repeat&&!(held&bit))blocked|=bit; // late attach/reconnect: wait for release
        held|=bit;
    }
    void axis(unsigned axis,float value){
        if(!std::isfinite(value))value=0;
        value=std::clamp(value,-1.f,1.f);
        if(axis<2){(axis?y:x)=static_cast<int>(value*32767.f);if(std::max(std::abs(x),std::abs(y))<9000)blockStick=false;}
        else if(axis==4){
            // One page per right-stick tilt. Hysteresis prevents chatter near
            // the activation threshold, while short flicks retain their edge.
            constexpr float release=9000.f/32767.f;
            button(Pad::PageUp,value>((held&Pad::PageUp)?release:.5f),false);
            button(Pad::PageDown,value<-((held&Pad::PageDown)?release:.5f),false);
        }
        else {bool down=value>.5f;bool& old=axis==2?left:right;bool& block=axis==2?blockLeft:blockRight;bool& edge=axis==2?leftEdge:rightEdge;
            if(!down)block=false;if(enabled&&down&&!old&&!block)edge=true;old=down;}
    }
    ControllerSample sample(){
        ControllerSample s;if(enabled)s={static_cast<uint16_t>(held&~blocked),edges,blockStick?0:x,blockStick?0:y,left&&!blockLeft,right&&!blockRight,leftEdge,rightEdge};
        edges=0;leftEdge=rightEdge=false;return s;
    }
};
}

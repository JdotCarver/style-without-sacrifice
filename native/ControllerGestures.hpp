#pragma once
#include <array>
#include <cmath>
#include <cstdint>
namespace Wardrobe {
// Down/up ownership survives page/focus transitions. This is separate from UI
// action state: an already-forwarded press must receive its downstream release,
// and a captured press must never leak a release into a later game/menu context.
struct ControllerGestures {
    uint32_t down{},forwarded{},captured{};bool capturing{};
    std::array<float,4> axes{};std::array<bool,4> neutral{true,true,true,true};
    uint32_t enter(bool capture){
        if(capture==capturing)return 0;capturing=capture;neutral.fill(true);
        uint32_t drain{};if(capture)for(unsigned i=0;i<4;++i)if(axes[i]!=0){drain|=1u<<i;axes[i]=0;}
        return drain;
    }
    // True means this gesture belongs to Wardrobe, even for its later release.
    bool button(unsigned index,bool pressed,bool repeat){
        uint32_t bit=1u<<index;
        if(!pressed){bool ours=(captured&bit)!=0;down&=~bit;forwarded&=~bit;captured&=~bit;return ours;}
        if(down&bit)return (captured&bit)!=0;
        down|=bit;
        // Unknown repeats may predate attachment; never turn them into actions.
        if(repeat){if(captured&bit)return true;forwarded|=bit;return false;}
        captured&=~bit;
        if(capturing){captured|=bit;return true;}forwarded|=bit;return false;
    }
    float axis(unsigned index,float value){
        if(!std::isfinite(value))value=0;
        if(!capturing){axes[index]=value;return value;}
        float threshold=index<2?9000.f/32767.f:.5f;
        if(std::abs(value)<=threshold)neutral[index]=false;
        return neutral[index]?0:value;
    }
    uint32_t cancel(){uint32_t release=forwarded;forwarded=0;down=0;capturing=false;neutral.fill(true);return release;}
};
}

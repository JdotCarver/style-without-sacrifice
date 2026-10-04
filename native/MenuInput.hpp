#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>

namespace Wardrobe {
// Dominant-axis navigation, with hysteresis and a slower first repeat. A long
// frame advances once instead of replaying a burst of missed repeats.
struct StickNavigation {
    int direction{};uint64_t next{};
    void reset(){direction=0;next=0;}
    int poll(int x,int y,uint64_t now){
        int ax=std::abs(x),ay=std::abs(y),threshold=direction?9000:14000;
        int current=std::max(ax,ay)<threshold?0:ax>ay?(x>0?1:-1):(y>0?-2:2);
        if(!current){reset();return 0;}
        if(current!=direction){direction=current;next=now+300;return current;}
        if(now<next)return 0;next=now+110;return current;
    }
};
}

#pragma once
#include <algorithm>

namespace Wardrobe::Layout {
struct Rect {double x,y,w,h;};
inline constexpr double width=1920,height=1000;
inline constexpr unsigned columns=6,rows=6,pageSize=columns*rows;
inline constexpr Rect catalog{128,180,636,636};
inline constexpr Rect doll{768,-24,1040,1040};
inline constexpr Rect summary{1600,82,230,710};
inline constexpr Rect dialog{550,242,820,516};
inline constexpr Rect tile(unsigned i){return {double(i%columns)*106,double(i/columns)*106,94,94};}
inline constexpr unsigned pages(unsigned count){return std::max(1u,(count+pageSize-1)/pageSize);}
inline constexpr unsigned count(unsigned total,unsigned page){return page<pages(total)?std::min(pageSize,total-std::min(total,page*pageSize)):0;}
}

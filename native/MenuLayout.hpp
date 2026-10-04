#pragma once
#include <algorithm>

namespace Wardrobe::Layout {
struct Rect {double x,y,w,h;};
inline constexpr double width=1920,height=1000;
inline constexpr unsigned columns=6,rows=6,pageSize=columns*rows;
inline constexpr Rect catalog{128,166,636,636};
inline constexpr Rect doll{790,-48,1000,1000};
inline constexpr Rect summary{1600,56,296,692};
inline constexpr Rect dialog{550,242,820,516};
inline constexpr Rect lookName{128,804,636,58};
inline constexpr Rect pagePrevious{128,866,202,44},pageNext{562,866,202,44},pageCount{385,876,110,34};
inline constexpr Rect category(unsigned i){return {146+double(i)*50,42,44,44};}
inline constexpr Rect equipment(unsigned i){return {1600+(i==2||i==3?104.:0.),166+(i==1||i==3?104.:i==4?208.:0.),94,94};}
inline constexpr Rect prompts[]={{64,920,200,44},{770,920,194,44},{974,920,204,44},{1188,920,214,44},{1412,920,214,44},{1636,920,240,44}};
inline constexpr double promptFont=18,promptGlyph=28;
inline constexpr Rect tile(unsigned i){return {double(i%columns)*106,double(i/columns)*106,94,94};}
inline constexpr unsigned pages(unsigned count){return std::max(1u,(count+pageSize-1)/pageSize);}
inline constexpr unsigned count(unsigned total,unsigned page){return page<pages(total)?std::min(pageSize,total-std::min(total,page*pageSize)):0;}
}

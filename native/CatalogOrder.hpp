#pragma once
#include "MenuLayout.hpp"
#include <vector>

namespace Wardrobe {
enum class LookGroup { Options, Player, NPC };
struct CatalogPage {unsigned begin{},count{};LookGroup group=LookGroup::Options;};
inline std::vector<CatalogPage> groupPages(unsigned options,unsigned player,unsigned npc){
    std::vector<CatalogPage> pages;
    auto append=[&](unsigned begin,unsigned count,LookGroup group){
        for(unsigned used=0;used<count;used+=Layout::pageSize)pages.push_back({begin+used,std::min(Layout::pageSize,count-used),group});
    };
    if(player)append(0,options+player,LookGroup::Player);
    if(npc)append(player?options+player:0,npc+(player?0:options),LookGroup::NPC);
    if(pages.empty())pages.push_back({0,options,LookGroup::Options});
    return pages;
}
template<class Look> bool lookOrder(const Look& a,const Look& b){
    if(a.hasPreviewIcon!=b.hasPreviewIcon)return a.hasPreviewIcon;
    if(a.sortKey!=b.sortKey)return a.sortKey<b.sortKey;
    if(a.label!=b.label)return a.label<b.label;
    return a.row<b.row;
}
}

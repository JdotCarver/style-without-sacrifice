#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace Wardrobe {
enum class Slot : unsigned { Torso, Legs, Gauntlets, Feet, Weapon, Count };
constexpr size_t slotCount=static_cast<size_t>(Slot::Count);
inline constexpr const wchar_t* slotNames[]={L"Torso",L"Legs",L"Gauntlets",L"Feet",L"Weapon"};
inline bool hideAllowed(Slot s){return s==Slot::Gauntlets||s==Slot::Feet||s==Slot::Weapon;}
struct Choice {
    enum class Mode { Original, Look, Hidden } mode{};
    std::wstring row;
    bool operator==(const Choice&)const=default;
};
using Outfit=std::array<Choice,slotCount>;
struct Model {
    std::array<Outfit,2> sets{};
    std::array<std::optional<Outfit>,3> presets{};
    std::set<std::wstring> collected;
    bool allLooks{};
    unsigned displayedSet{}, category{}, page{}, selected{};
    uint64_t revision{};
    bool choose(Slot slot,Choice choice){
        auto index=static_cast<size_t>(slot);
        if(index>=slotCount||displayedSet>1||(choice.mode==Choice::Mode::Hidden&&!hideAllowed(slot)))return false;
        if(choice.mode==Choice::Mode::Look&&(choice.row.empty()||choice.row.size()>512))return false;
        if(choice.mode!=Choice::Mode::Look)choice.row.clear();
        if(sets[displayedSet][index]==choice)return false;
        sets[displayedSet][index]=std::move(choice);++revision;return true;
    }
    bool reset(){if(sets[displayedSet]==Outfit{})return false;sets[displayedSet]={};++revision;return true;}
    bool save(unsigned i){if(i>=presets.size())return false;presets[i]=sets[displayedSet];++revision;return true;}
    bool load(unsigned i){if(i>=presets.size()||!presets[i])return false;sets[displayedSet]=*presets[i];++revision;return true;}
    void switchSet(unsigned i){displayedSet=std::min(i,1u);page=selected=0;}
    bool learn(const std::wstring& key){if(key.empty()||key.size()>512||collected.size()>=16384)return false;if(!collected.insert(key).second)return false;++revision;return true;}
};
struct Settings {bool enabled=true;bool debugLogging=false;unsigned openKey=0;};
// An event burst coalesces into one bounded continuation; no retry work remains
// after success or exhaustion. A new lifecycle event starts a fresh window.
struct Work {
    unsigned attempts{};bool pending{};uint64_t due{};uint64_t generation{};
    void request(uint64_t now){if(!pending){attempts=0;due=now;pending=true;}}
    void cancel(){pending=false;attempts=0;++generation;}
    bool ready(uint64_t now)const{return pending&&now>=due;}
    void finish(bool success,uint64_t now){if(success||++attempts>=12)pending=false;else due=now+250;}
};
}

#pragma once
#include "Model.hpp"
#include "Reflection.hpp"
#include <Unreal/Hooks/Hooks.hpp>
#include <filesystem>
#include <functional>

namespace Wardrobe {
struct Look {Slot slot;std::wstring row,label,itemPath;Ref item;bool hasPreviewIcon{};std::string sortKey;};
struct Runtime {
    Model model;Settings settings;std::filesystem::path directory;
    Ref player,controller,inventory,appearance,doll,dollAppearance,hub;
    Work attachWork,refreshWork;uint64_t frame{},now{};
    bool playerRefresh{},previewRefresh{};
    bool menuOpen{},catalogReady{},playerReady{},dirty{},shuttingDown{},persistenceBlocked{};
    unsigned activeSet{};
    std::vector<Look> looks;
    uint64_t catalogSteps{},refreshes{},nativeOverrides{},weaponEvents{},workMicros{},maxWorkMicros{},inventorySnapshots{},inventoryItems{};
};
extern Runtime runtime;
void readStore();bool writeStore();void pollStore();void finishStore();bool storePending();
bool startCosmetics();void stopCosmetics();void resetCosmetics();
bool attach(UObject*);bool refresh();
void beginCatalog();bool stepCatalog();
void selectLook(Slot,const Choice&);void setDoll(UObject*);
void observeWeapon(UObject*);
void initializeMenu();void menuHub(UObject*);void openMenu();void closeMenu();void stepMenu();void resetMenu();
void menuScriptPre(UObject*,UFunction*,void*,RC::Unreal::Hook::TCallbackIterationData<void>&);
void menuScriptPost(UObject*,UFunction*,void*,RC::Unreal::FFrame*);
void menuRedraw();
bool menuPending();bool catalogPending();void inventoryChanged(bool equipment=false);
void requestRefresh(bool player=true,bool preview=true);bool inventoryPending();void stepInventory();
void inventoryAdded(UFunction*,void*);
void cancelCosmeticWork();
void configure(Settings);void requestOpen();void tick();void stop();
void queueHub(UObject*);
}

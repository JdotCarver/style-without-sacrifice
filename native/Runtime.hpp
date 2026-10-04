#pragma once
#include "Model.hpp"
#include "Reflection.hpp"
#include <Unreal/Hooks/Hooks.hpp>
#include <filesystem>
#include <functional>

namespace Wardrobe {
struct Look {Slot slot;std::wstring row,label,itemPath;};
struct Runtime {
    Model model;Settings settings;std::filesystem::path directory;
    Ref player,controller,inventory,appearance,doll,dollAppearance,hub;
    Work attachWork,refreshWork;uint64_t frame{},now{};
    bool menuOpen{},catalogReady{},dirty{},shuttingDown{},persistenceBlocked{};
    unsigned activeSet{};
    std::vector<Look> looks;
    uint64_t catalogSteps{},refreshes{},nativeOverrides{},workMicros{};
};
extern Runtime runtime;
void readStore();bool writeStore();
bool startCosmetics();void stopCosmetics();void resetCosmetics();
bool attach(UObject*);bool refresh();
void beginCatalog();bool stepCatalog();void learnInventory();
void selectLook(Slot,const Choice&);void setDoll(UObject*);
void observeWeapon(UObject*);
void initializeMenu();void menuHub(UObject*);void openMenu();void closeMenu();void stepMenu();void resetMenu();
void menuScriptPre(UObject*,UFunction*,void*,RC::Unreal::Hook::TCallbackIterationData<void>&);
void menuScriptPost(UObject*,UFunction*,void*);
void menuRedraw();
bool menuPending();bool catalogPending();void inventoryChanged();
void configure(Settings);void requestOpen();void tick();void stop();
}

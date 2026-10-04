#include "Runtime.hpp"
#include <Mod/CppUserModBase.hpp>
#include <LuaMadeSimple/LuaMadeSimple.hpp>
#include <Unreal/FFrame.hpp>
#include <Unreal/Core/Windows/AllowWindowsPlatformTypes.hpp>
#include <Windows.h>
#include <atomic>
#include <chrono>
#include <mutex>

namespace Wardrobe {
Runtime runtime;
namespace {
std::atomic_bool opening{};std::atomic_bool configured{};
std::mutex settingsMutex;Settings requested;
Ref pendingPlayer,pendingHub;std::atomic_bool active{},hubQueued{};std::mutex hubMutex;uint64_t lastSummary{},saveDue{};
Ref weaponClass,pawnClass;
bool openAfterAttach{};
std::vector<Hook::GlobalCallbackId> callbacks;
struct FunctionHook {Ref fn;std::pair<int,int> ids;};std::vector<FunctionHook> functionHooks;
void resetSession(){
    openAfterAttach=false;
    closeMenu();resetMenu();runtime.attachWork.cancel();runtime.refreshWork.cancel();resetCosmetics();
    runtime.player={};runtime.controller={};runtime.inventory={};runtime.appearance={};runtime.doll={};runtime.dollAppearance={};runtime.hub={};pendingPlayer={};clearReflection();
}
void requestPlayer(UObject* pawn){if(pawn){if(runtime.attachWork.pending&&pendingPlayer.matches(pawn))return;pendingPlayer=Ref(pawn);runtime.attachWork.cancel();runtime.attachWork.request(runtime.now);}}
void registerFunction(const wchar_t* path,std::function<void(UnrealScriptFunctionCallableContext&,UFunction*)> callback){
    auto fn=static_cast<UFunction*>(find(path));if(!fn){warn(std::wstring(L"Optional event unavailable: ")+path);return;}
    try{auto ids=UObjectGlobals::RegisterHook(fn,{},[callback,fn](UnrealScriptFunctionCallableContext& ctx,void*){if(active)callback(ctx,fn);},nullptr);functionHooks.push_back({Ref(fn),ids});}
    catch(...){warn(std::wstring(L"Could not register event: ")+path);}
}
void setup(){
    if(active)return;active=true;initializeReferences();initializeMenu();startCosmetics();readStore();weaponClass=Ref(find(L"/Script/DogwoodCombat.WeaponBase"));pawnClass=Ref(find(L"/Script/Engine.Pawn"));
    Hook::FCallbackOptions options;options.OwnerModName=L"WardrobeTransmog";
    options.HookName=L"Bounded UI and event work";
    callbacks.push_back(Hook::RegisterEngineTickPostCallback([](auto&,auto*,float,bool){tick();},options));
    options.HookName=L"Player and weapon lifecycle";
    callbacks.push_back(Hook::RegisterBeginPlayPostCallback([](auto&,AActor* actor){
        if(!active)return;auto p=reinterpret_cast<UObject*>(actor);
        // No global discovery here. Only player candidates and owned weapons
        // are retained, with deletion-aware identities.
        if(auto cls=static_cast<UClass*>(pawnClass.get());cls&&p->IsA(cls)){auto pc=object(p,L"Controller");if(pc&&object(pc,L"Player"))requestPlayer(p);}
        else if(auto cls=static_cast<UClass*>(weaponClass.get());cls&&p->IsA(cls))observeWeapon(p);
    },options));
    options.HookName=L"World teardown";
    callbacks.push_back(Hook::RegisterEndPlayPreCallback([](auto&,AActor* actor,EEndPlayReason){if(active&&runtime.player.matches(reinterpret_cast<UObject*>(actor)))resetSession();},options));
    // ProcessLocalScriptFunction is filtered to the current hub's exact bound
    // function pointers. No Lua is invoked by these native callbacks.
    options.HookName=L"Wardrobe hub selection";
    callbacks.push_back(Hook::RegisterProcessLocalScriptFunctionPreCallback([](auto& info,UObject* owner,FFrame& frame,void*){if(active)menuScriptPre(owner,frame.Node(),frame.Locals(),info);},options));
    callbacks.push_back(Hook::RegisterProcessLocalScriptFunctionPostCallback([](auto&,UObject* owner,FFrame& frame,void*){if(active)menuScriptPost(owner,frame.Node(),frame.Locals(),&frame);},options));
    for(auto id:callbacks)if(id==Hook::ERROR_ID){warn(L"A required lifecycle hook could not be registered; restart with the required UE4SS native callback support.");stop();return;}
    // Activation fires after the widget tree exists, including pooled hubs.
    // There is no Wardrobe callback on every UObject construction.
    registerFunction(L"/Script/CommonUI.CommonActivatableWidget:ActivateWidget",[](auto& c,auto*){
        static const FName hubName(L"WBP_Window_GameHub_C");auto h=c.Context;
        if(h&&h->GetClassPrivate()->GetFName()==hubName)queueHub(h);
    });
    registerFunction(L"/Script/Engine.PlayerController:ClientRestart",[](auto& c,auto*){if(auto p=object(c.Context,L"AcknowledgedPawn"))requestPlayer(p);});
    for(auto name:{L"SetActiveLoadout",L"TryAddItem",L"TryAddAndEquipItem",L"TryEquipItem",L"TryEquipItemInSlot",L"UnequipItem",L"RequestItemUnequip"}){
        auto path=std::wstring(L"/Script/DogwoodInventory.InventoryComponent:")+name;
        bool equipment=std::wstring_view(name)!=L"TryAddItem";
        bool addition=std::wstring_view(name)==L"TryAddItem"||std::wstring_view(name)==L"TryAddAndEquipItem";
        registerFunction(path.c_str(),[equipment,addition](auto& c,auto* fn){if(runtime.inventory.matches(c.Context)){if(addition)inventoryAdded(fn,c.TheStack.Locals());inventoryChanged(equipment);}});
    }
    // One startup fallback supports late loading. Later recovery is driven by
    // ClientRestart/BeginPlay or an explicit open request.
    if(auto controller=UObjectGlobals::FindFirstOf(L"PlayerController"))if(object(controller,L"Player"))requestPlayer(object(controller,L"AcknowledgedPawn"));
}
}
void configure(Settings settings){std::lock_guard lock(settingsMutex);requested=settings;configured=true;}
void requestOpen(){opening=true;}
void queueHub(UObject* h){std::lock_guard lock(hubMutex);pendingHub=Ref(h);hubQueued=true;}
void tick(){
    if(!active)return;++runtime.frame;
    bool requestedOpen=opening.exchange(false),apply=configured.exchange(false);
    if(!requestedOpen&&!apply&&!hubQueued&&!runtime.attachWork.pending&&!runtime.refreshWork.pending&&!runtime.menuOpen&&!menuPending()&&!catalogPending()&&!inventoryPending()&&!storePending()&&!runtime.dirty)return;
    runtime.now=GetTickCount64();
    auto started=logging?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
    const wchar_t* operation=L"Event processing";
    try{
        if(hubQueued.exchange(false)){Ref h;{std::lock_guard lock(hubMutex);h=pendingHub;pendingHub={};}if(auto p=h.get())menuHub(p);}
        if(apply){Settings s;{std::lock_guard lock(settingsMutex);s=requested;}bool changed=runtime.settings.enabled!=s.enabled;if(!logging&&s.debugLogging)started=std::chrono::steady_clock::now();runtime.settings=s;logging=s.debugLogging;if(changed){if(!s.enabled)closeMenu();requestRefresh();}if(logging)trace(s.enabled?L"Settings applied: enabled.":L"Settings applied: disabled.");}
        if(runtime.player.address&&!runtime.player){resetSession();return;}
        bool busy=false;
        if(runtime.attachWork.ready(runtime.now)){
            operation=L"Player attachment";bool ok=false;busy=true;
            try{ok=attach(pendingPlayer.get());}catch(const std::exception& e){failure(operation,e);}
            runtime.attachWork.finish(ok,runtime.now);
            if(ok&&openAfterAttach){opening=true;openAfterAttach=false;}
            if(!ok&&!runtime.attachWork.pending){openAfterAttach=false;opening=false;warn(L"Player attachment stopped after 12 attempts; a player event or Wardrobe shortcut can retry.");}
        }
        if(requestedOpen&&busy&&(runtime.attachWork.pending||runtime.playerReady))opening=true;
        if(requestedOpen&&!busy&&runtime.settings.enabled){
            operation=L"Wardrobe open";
            if(!runtime.playerReady){
                openAfterAttach=true;
                if(!runtime.attachWork.pending){auto pawn=runtime.player.get();if(!pawn)if(auto pc=UObjectGlobals::FindFirstOf(L"PlayerController"))if(object(pc,L"Player"))pawn=object(pc,L"AcknowledgedPawn");requestPlayer(pawn);}
            }else if(runtime.menuOpen)closeMenu();else openMenu();busy=true;
        }
        // A refresh, inventory snapshot/slice, catalog slice or UI build gets
        // its own frame. Do not add several independent budgets together.
        if(!busy&&runtime.refreshWork.ready(runtime.now)){operation=L"Appearance refresh";runtime.refreshWork.finish(refresh(),runtime.now);busy=true;}
        bool ui=runtime.menuOpen||menuPending();
        if(!busy&&ui&&runtime.frame%2==0){operation=L"Wardrobe menu";stepMenu();busy=true;}
        if(!busy&&catalogPending()){operation=L"Appearance catalog";stepCatalog();busy=true;}
        if(!busy&&inventoryPending()){operation=L"Collection update";stepInventory();busy=true;}
        if(!busy&&ui){operation=L"Wardrobe menu";stepMenu();}
        pollStore();
        if(runtime.dirty){if(!saveDue)saveDue=runtime.now+1500;if(runtime.now>=saveDue){writeStore();saveDue=0;}}else saveDue=0;
        if(logging){auto elapsed=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-started).count());runtime.workMicros+=elapsed;runtime.maxWorkMicros=std::max(runtime.maxWorkMicros,elapsed);if(runtime.now-lastSummary>=10000){lastSummary=runtime.now;trace(L"Work totals: catalog steps="+std::to_wstring(runtime.catalogSteps)+L", refreshes="+std::to_wstring(runtime.refreshes)+L", clothing overrides="+std::to_wstring(runtime.nativeOverrides)+L", inventory snapshots="+std::to_wstring(runtime.inventorySnapshots)+L", inventory entries="+std::to_wstring(runtime.inventoryItems)+L", work us="+std::to_wstring(runtime.workMicros)+L", max tick us="+std::to_wstring(runtime.maxWorkMicros));}}
    }catch(const std::exception& e){
        failure(operation,e);opening=false;openAfterAttach=false;runtime.attachWork.cancel();cancelCosmeticWork();
        try{closeMenu();}catch(const std::exception& closeError){failure(L"Closing failed menu",closeError);}resetMenu();
    }
}
void stop(){
    if(!active)return;active=false;runtime.shuttingDown=true;writeStore();finishStore();
    // Teardown never invokes gameplay or UI functions from the loader thread.
    for(auto id:callbacks)Hook::UnregisterCallback(id);callbacks.clear();
    for(auto& hook:functionHooks)if(auto fn=static_cast<UFunction*>(hook.fn.get()))UObjectGlobals::UnregisterHook(fn,hook.ids);functionHooks.clear();
    shutdownReferences();stopCosmetics();
}
}
using namespace RC;
static_assert(sizeof(CppUserModBase)==192);
static_assert(sizeof(Unreal::Hook::FCallbackOptions)==72);
class WardrobeMod final:public CppUserModBase {
public:
    WardrobeMod(){ModName=L"Style Without Sacrifice - Your Transmogrification Wardrobe";ModVersion=L"0.1.0";ModAuthors=L"my-mods";ModDescription=L"An independent wardrobe tab with separate day and night outfits.";}
    void on_lua_start(StringViewType name,LuaMadeSimple::Lua& lua,LuaMadeSimple::Lua&,LuaMadeSimple::Lua&,LuaMadeSimple::Lua*)override{
        if(name!=L"WardrobeTransmog")return;
        lua.register_function("_WCConfigure",[](const auto& l){Wardrobe::Settings s;s.enabled=l.get_integer(1)!=0;s.openKey=static_cast<unsigned>(std::clamp<int64_t>(l.get_integer(1),0,3));s.debugLogging=l.get_integer(1)!=0;Wardrobe::configure(s);return 0;});
        lua.register_function("_WCStart",[](const auto& l){
            auto utf=l.get_string(1);int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,utf.data(),static_cast<int>(utf.size()),nullptr,0);std::wstring path(n,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,utf.data(),static_cast<int>(utf.size()),path.data(),n);Wardrobe::runtime.directory=path;Wardrobe::setup();return 0;
        });
        lua.register_function("_WCOpen",[](const auto&){Wardrobe::requestOpen();return 0;});
    }
    ~WardrobeMod()override{Wardrobe::stop();}
    void on_lua_stop(StringViewType name,LuaMadeSimple::Lua&,LuaMadeSimple::Lua&,LuaMadeSimple::Lua&,LuaMadeSimple::Lua*)override{
        if(name==L"WardrobeTransmog"){Wardrobe::Settings s;s.enabled=false;Wardrobe::configure(s);}
    }
};
extern "C" __declspec(dllexport) CppUserModBase* start_mod(){return new WardrobeMod;}
extern "C" __declspec(dllexport) void uninstall_mod(CppUserModBase* mod){delete mod;}

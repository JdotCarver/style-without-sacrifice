#include "Runtime.hpp"
#include "ControllerInput.hpp"
#include "ControllerGestures.hpp"
#include "ControllerContract.hpp"
#include <Unreal/UEnum.hpp>
#include <MinHook.h>
#include <atomic>
#include <cstring>

namespace Wardrobe {
namespace {
// FGenericApplicationMessageHandler's device-aware overloads: an eight-byte
// FName is passed by value in RDX, IDs in R8D/R9D, value/repeat on the stack.
// Do not use a TCHAR* or confuse these with the four-argument legacy overloads.
using Analog=bool(*)(void*,uint64_t,int32_t,int32_t,float);
using Button=bool(*)(void*,uint64_t,int32_t,int32_t,bool);
Analog originalAnalog{};Button originalPressed{},originalReleased{};
ControllerContract::Binding binding;std::array<void*,3> installed{};
std::atomic_bool running{};DWORD gameThread{};SRWLOCK callbackLock=SRWLOCK_INIT;bool reentrant{},everEnabled{};
thread_local unsigned callbackDepth{};
struct CallbackGuard {bool outer;CallbackGuard():outer(callbackDepth++==0){if(outer)AcquireSRWLockShared(&callbackLock);}~CallbackGuard(){if(!--callbackDepth)ReleaseSRWLockShared(&callbackLock);}};
struct InputGuard {InputGuard(){reentrant=true;}~InputGuard(){reentrant=false;}};
ControllerState state;Ref owner,connectionEnum;int32_t device=-1;int connectedValue=-1;
struct DeviceGestures {int32_t device=-1,user=-1;ControllerGestures gestures;bool valid{};};std::array<DeviceGestures,4> devices;
uint64_t lastDiagnostic{},received{},consumed{},rejected{},reconnects{},deviceQueries{},enumScans{};bool failed{},cancelled{};
struct Key {uint64_t name{};uint16_t button{};int axis=-1;};
std::array<Key,21> keys;
void key(unsigned i,const wchar_t* text,uint16_t button,int axis=-1){
    static_assert(sizeof(FName)==8);FName name(text);std::memcpy(&keys[i].name,&name,8);keys[i].button=button;keys[i].axis=axis;
}
const Key* lookup(uint64_t name){for(const auto& k:keys)if(k.name==name)return &k;return nullptr;}
void deviceArgument(Call& c,int32_t id){
    auto p=c.field(L"DeviceId");
    if(!p||!p->IsA<FStructProperty>()||p->GetSize()!=4||!static_cast<FStructProperty*>(p)->GetStruct()||static_cast<FStructProperty*>(p)->GetStruct()->GetFName()!=FName(L"InputDeviceId"))
        throw std::runtime_error("InputDeviceLibrary.DeviceId must be the four-byte InputDeviceId struct");
    std::memcpy(c.value(L"DeviceId"),&id,4);
}
bool ownsDevice(int32_t id){
    if(id<0||!runtime.controller)return false;
    auto library=find(L"/Script/Engine.Default__InputDeviceLibrary");
    Call connection(library,L"GetInputDeviceConnectionState");deviceArgument(connection,id);
    auto p=connection.fn->GetReturnProperty();
    if(!p||!p->IsA<FEnumProperty>()||p->GetSize()!=1)throw std::runtime_error("Input device connection enum unavailable");
    auto type=static_cast<FEnumProperty*>(p)->GetEnum();
    if(!connectionEnum.matches(type)){connectedValue=-1;connectionEnum=Ref(type);if(logging)++enumScans;
        if(type)for(auto v:type->ForEachName())if(v.Key.ToString()==L"EInputDeviceConnectionState::Connected")connectedValue=static_cast<int>(v.Value);
    }
    if(connectedValue<0)throw std::runtime_error("Input device Connected enum value unavailable");
    if(logging)++deviceQueries;connection.invoke();if(connection.resultInteger()!=connectedValue)return false;
    Call controller(library,L"GetPlayerControllerFromInputDevice");deviceArgument(controller,id);
    auto result=controller.fn->GetReturnProperty();if(!result||!result->IsA<FObjectProperty>()||result->GetSize()!=sizeof(void*))throw std::runtime_error("Input device controller result unavailable");
    if(logging)++deviceQueries;controller.invoke();return runtime.controller.matches(controller.resultObject());
}
void disableOnFailure(const std::exception& e){
    state.clear();device=-1;owner={};failed=true;cancelled=true;
    failure(L"Engine controller input (keyboard and mouse remain available)",e);
}
bool eligible(void* handler){
    return running.load(std::memory_order_acquire)&&GetCurrentThreadId()==gameThread&&!reentrant&&ControllerContract::current(binding,handler);
}
void drain(DeviceGestures& slot,bool capture,bool cancel=false){
    auto self=binding.application&&*binding.application?static_cast<unsigned char*>(*binding.application)+0x148:nullptr;
    if(!self)return;
    auto axes=slot.gestures.enter(capture);
    if(cancel){
        auto releases=slot.gestures.cancel();
        for(unsigned i=0;i<keys.size();++i)if(releases&(1u<<i))originalReleased(self,keys[i].name,slot.user,slot.device,false);
        for(unsigned i=0;i<4;++i)if(slot.gestures.axes[i]!=0){axes|=1u<<i;slot.gestures.axes[i]=0;}
    }
    for(unsigned i=0;i<4;++i)if(axes&(1u<<i))originalAnalog(self,keys[11+i].name,slot.user,slot.device,0);
}
DeviceGestures* prepare(int32_t id,int32_t user){
    if(failed){for(auto& d:devices)if(d.device==id){drain(d,false);return &d;}return nullptr;}
    if(!owner.matches(runtime.controller.get())){
        state.clear();owner=runtime.controller;device=-1;cancelled=true;
        for(auto& d:devices)if(d.device>=0){drain(d,false,true);d.valid=false;}
    }
    auto found=std::find_if(devices.begin(),devices.end(),[&](const auto& d){return d.device==id;});
    if(found==devices.end()){
        if(!ownsDevice(id)){if(logging)++rejected;return nullptr;}
        found=std::find_if(devices.begin(),devices.end(),[](const auto& d){return d.device<0;});
        if(found==devices.end())found=std::find_if(devices.begin(),devices.end(),[](const auto& d){return d.device!=device&&!d.gestures.down&&!d.gestures.captured&&std::all_of(d.gestures.axes.begin(),d.gestures.axes.end(),[](float v){return v==0;});});
        if(found==devices.end())return nullptr;
        drain(*found,false,true);*found={};found->device=id;found->user=user;found->valid=true;
        if(logging)++reconnects;
    }
    if(!found->valid)found->valid=ownsDevice(id);
    bool capture=found->valid&&menuControllerFocused();
    if(found->gestures.capturing&&!capture){cancelled=true;if(id==device)state.suspend();}
    drain(*found,capture);
    if(id==device||device<0)state.enable(capture);return &*found;
}
void adoptDevice(int32_t id){
    if(device!=id){state.clear();device=id;cancelled=true;}state.enable(true);
}
bool analog(void* self,uint64_t name,int32_t user,int32_t id,float value){
    CallbackGuard callback;auto k=lookup(name);
    if(k&&k->axis>=0&&eligible(self))try{
        InputGuard guard;if(logging)++received;
        if(auto slot=prepare(id,user)){
            auto v=slot->gestures.axis(static_cast<unsigned>(k->axis),value);
            if(slot->gestures.capturing){
                // Most-recent meaningful input owns navigation. Another owned
                // device's idle/neutral events cannot repeatedly steal it.
                if(std::abs(v)>(k->axis<2?9000.f/32767.f:.5f))adoptDevice(id);
                if(device==id)state.axis(static_cast<unsigned>(k->axis),v);
                if(logging)++consumed;return true;
            }
        }
    }catch(const std::exception& e){disableOnFailure(e);}
    return originalAnalog(self,name,user,id,value);
}
bool button(void* self,uint64_t name,int32_t user,int32_t id,bool repeat,bool down){
    CallbackGuard callback;auto k=lookup(name);
    if(k&&k->axis<0&&eligible(self))try{
        InputGuard guard;if(logging)++received;
        if(auto slot=prepare(id,user))if(slot->gestures.button(static_cast<unsigned>(k-keys.data()),down,repeat)){
            if(slot->gestures.capturing&&k->button&&down&&!repeat)adoptDevice(id);
            // Retire held/blocked bits on release even if the page lost focus
            // since the press. Otherwise the next fresh press has no edge.
            if(device==id&&(slot->gestures.capturing||!down))state.button(k->button,down,repeat);
            if(logging)++consumed;return true;
        }
    }catch(const std::exception& e){disableOnFailure(e);}
    return (down?originalPressed:originalReleased)(self,name,user,id,repeat);
}
bool pressed(void* self,uint64_t name,int32_t user,int32_t id,bool repeat){return button(self,name,user,id,repeat,true);}
bool released(void* self,uint64_t name,int32_t user,int32_t id,bool repeat){return button(self,name,user,id,repeat,false);}
}
void startControllerInput(){
    if(running||everEnabled)return;gameThread=GetCurrentThreadId();failed=false;
    key(0,L"Gamepad_DPad_Up",Pad::Up);key(1,L"Gamepad_DPad_Down",Pad::Down);key(2,L"Gamepad_DPad_Left",Pad::Left);key(3,L"Gamepad_DPad_Right",Pad::Right);
    key(4,L"Gamepad_Special_Right",Pad::Menu);key(5,L"Gamepad_LeftThumbstick",Pad::LStick);key(6,L"Gamepad_RightThumbstick",Pad::RStick);
    key(7,L"Gamepad_FaceButton_Bottom",Pad::A);key(8,L"Gamepad_FaceButton_Right",Pad::B);key(9,L"Gamepad_FaceButton_Left",Pad::X);key(10,L"Gamepad_FaceButton_Top",Pad::Y);
    key(11,L"Gamepad_LeftX",0,0);key(12,L"Gamepad_LeftY",0,1);key(13,L"Gamepad_LeftTriggerAxis",0,2);key(14,L"Gamepad_RightTriggerAxis",0,3);
    // Devices emit these digital keys as well as axes. Consume them without a
    // second action so Slate cannot also navigate the focused stock widget.
    key(15,L"Gamepad_LeftStick_Up",0);key(16,L"Gamepad_LeftStick_Down",0);key(17,L"Gamepad_LeftStick_Left",0);key(18,L"Gamepad_LeftStick_Right",0);
    key(19,L"Gamepad_LeftTrigger",0);key(20,L"Gamepad_RightTrigger",0);
    // Stock bumpers remain available for hub tab navigation.
    std::wstring error;binding=ControllerContract::resolve(error);
    if(!binding.targets[0]){warn(L"Engine controller input unavailable: "+error+L". Keyboard and mouse remain available.");return;}
    auto initialized=MH_Initialize();if(initialized!=MH_OK&&initialized!=MH_ERROR_ALREADY_INITIALIZED){warn(L"Engine controller hook service unavailable.");return;}
    void* hooks[]={reinterpret_cast<void*>(analog),reinterpret_cast<void*>(pressed),reinterpret_cast<void*>(released)};
    void** originals[]={reinterpret_cast<void**>(&originalAnalog),reinterpret_cast<void**>(&originalPressed),reinterpret_cast<void**>(&originalReleased)};
    for(size_t i=0;i<3;++i){
        if(MH_CreateHook(binding.targets[i],hooks[i],originals[i])!=MH_OK){stopControllerInput();warn(L"Slate controller entry already modified or cannot be hooked.");return;}installed[i]=binding.targets[i];
    }
    // Keep callback code/trampolines alive through loader-thread teardown. A
    // late entry after DisableHook can still forward safely, without touching
    // UObject state. At most three process-lifetime trampolines are retained.
    HMODULE pinned{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&analog),&pinned)){stopControllerInput();warn(L"Controller callback lifetime protection unavailable.");return;}
    everEnabled=true;running.store(true,std::memory_order_release);
    for(auto target:installed)if(MH_EnableHook(target)!=MH_OK){stopControllerInput();warn(L"Slate controller input hooks could not be enabled.");return;}
    if(logging)trace(L"Engine controller input ready: Slate device-aware analog/press/release, before CommonUI.");
}
void stopControllerInput(){
    running.store(false,std::memory_order_release);
    for(auto target:installed)if(target)MH_DisableHook(target);
    // Drain UObject work before the caller tears down its deletion listener.
    AcquireSRWLockExclusive(&callbackLock);ReleaseSRWLockExclusive(&callbackLock);
    if(!everEnabled)for(auto& target:installed)if(target){MH_RemoveHook(target);target=nullptr;}
}
void resetControllerInput(){
    state.suspend();failed=false;cancelled=true;
    for(auto& d:devices)if(d.device>=0)drain(d,false);
}
ControllerSample controllerSample(){
    if(!running||GetCurrentThreadId()!=gameThread||failed)return {.cancelled=true};
    try{
        bool focus=menuControllerFocused();
        bool changed=!owner.matches(runtime.controller.get());
        if(changed){state.clear();device=-1;owner=runtime.controller;cancelled=true;}
        InputGuard guard;
        for(auto& d:devices)if(d.device>=0){
            bool valid=!changed&&ownsDevice(d.device);
            if(d.valid&&!valid){drain(d,false,true);if(device==d.device){state.clear();device=-1;cancelled=true;}}
            d.valid=valid;drain(d,focus&&valid);
        }
        state.enable(focus);
        if(logging&&runtime.now-lastDiagnostic>=10000){lastDiagnostic=runtime.now;trace(L"Engine controller input: device="+std::to_wstring(device)+L", focused="+std::to_wstring(state.enabled)+L", received="+std::to_wstring(received)+L", consumed="+std::to_wstring(consumed)+L", rejected="+std::to_wstring(rejected)+L", device changes="+std::to_wstring(reconnects)+L", device queries="+std::to_wstring(deviceQueries)+L", enum scans="+std::to_wstring(enumScans)+L", UI ownership queries="+std::to_wstring(menuInputQueries()));}
        auto result=state.sample();result.cancelled=cancelled;cancelled=false;return result;
    }catch(const std::exception& e){disableOnFailure(e);return {.cancelled=true};}
}
}

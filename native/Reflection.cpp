#include "Reflection.hpp"
#include "Performance.hpp"
#include <DynamicOutput/Output.hpp>
#include <Unreal/UObjectArray.hpp>
#include <array>
#include <map>
#include <set>
#include <stdexcept>
#include <mutex>
#include <unordered_map>

namespace Wardrobe {
bool logging=false;
namespace {
constexpr size_t watchCount=8192;
std::array<std::atomic_bool,watchCount> interests;
std::unordered_map<int,std::weak_ptr<ObjectLife>> lives;
std::mutex lifeMutex;
std::atomic_bool alive=false;
struct Listener:FUObjectDeleteListener {
    void NotifyUObjectDeleted(const UObjectBase*,int32_t index)override{
        if(index<0||!interests[static_cast<unsigned>(index)%watchCount].load(std::memory_order_relaxed))return;
        std::lock_guard lock(lifeMutex);auto found=lives.find(index);if(found==lives.end())return;
        if(auto life=found->second.lock())life->alive=false;lives.erase(found);
    }
    void OnUObjectArrayShutdown()override{if(alive.exchange(false))FUObjectArray::RemoveUObjectDeleteListener(this);}
} listener;
struct PropertyBinding {Ref owner;FProperty* field{};};
struct FunctionBinding {Ref owner;Ref function;};
std::map<std::pair<UStruct*,std::wstring>,PropertyBinding> fields;
std::map<std::pair<UObject*,std::wstring>,FunctionBinding> functions;
std::map<std::wstring,Ref> objects;
std::set<std::wstring> warnings;
}
void warn(const std::wstring& s){if(warnings.size()<64&&warnings.insert(s).second)RC::Output::send(L"[WardrobeTransmog] "+s+L"\n");}
void trace(const std::wstring& s){if(logging)RC::Output::send(L"[WardrobeTransmog] "+s+L"\n");}
void failure(const wchar_t* action,const std::exception& error){
    std::wstring detail;for(auto p=error.what();*p&&detail.size()<384;++p)detail.push_back(static_cast<unsigned char>(*p));
    warn(std::wstring(action)+L": "+detail);
}
void initializeReferences(){if(!alive.exchange(true))FUObjectArray::AddUObjectDeleteListener(&listener);}
void shutdownReferences(){if(alive.exchange(false))FUObjectArray::RemoveUObjectDeleteListener(&listener);std::lock_guard lock(lifeMutex);for(auto& [index,value]:lives)if(auto life=value.lock())life->alive=false;lives.clear();}
void clearReflection(){fields.clear();functions.clear();objects.clear();warnings.clear();}
Ref::Ref(UObject* p){
    if(!alive||!p)return;auto i=p->GetInternalIndex();if(i<0)return;
    auto item=FUObjectArray::IndexToObject(i);
    if(!item||item->GetUObject()!=p||!FUObjectArray::IsValid(item,false))return;
    std::lock_guard lock(lifeMutex);auto& weak=lives[i];life=weak.lock();
    if(!life||!life->alive||life->address!=p){life=std::make_shared<ObjectLife>();life->address=p;weak=life;}
    interests[static_cast<unsigned>(i)%watchCount].store(true,std::memory_order_relaxed);
    address=p;index=i;serial=item->GetSerialNumber();
}
UObject* Ref::get()const{
    if(!alive||index<0||!life||!life->alive)return nullptr;
    auto item=FUObjectArray::IndexToObject(index);
    if(!item||item->GetUObject()!=address||!FUObjectArray::IsValid(item,false))return nullptr;
    if(serial&&item->GetSerialNumber()!=serial)return nullptr;return address;
}
FProperty* property(UStruct* owner,const std::wstring& name){
    if(!owner)return nullptr;auto key=std::make_pair(owner,name);auto it=fields.find(key);
    if(it!=fields.end()&&it->second.owner.matches(owner))return it->second.field;
    FProperty* p{};FName n(name.c_str());
    for(auto s=owner;s&&!p;s=s->GetSuperStruct())p=s->FindProperty(n);
    if(fields.size()<8192)fields[key]={Ref(owner),p};return p;
}
FProperty* property(UObject* owner,const std::wstring& n){return owner?property(static_cast<UStruct*>(owner->GetClassPrivate()),n):nullptr;}
UFunction* function(UObject* owner,const std::wstring& n){
    if(!owner)return nullptr;auto cls=static_cast<UObject*>(owner->GetClassPrivate());auto key=std::make_pair(cls,n);
    auto it=functions.find(key);if(it!=functions.end()&&it->second.owner.matches(cls))return static_cast<UFunction*>(it->second.function.get());
    auto fn=owner->GetFunctionByNameInChain(n.c_str());if(functions.size()<2048)functions[key]={Ref(cls),Ref(fn)};return fn;
}
UObject* find(const wchar_t* path){
    if(!path)return nullptr;
    auto it=objects.find(path);
    if(it!=objects.end()){if(auto value=it->second.get())return value;objects.erase(it);}
    auto value=UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,path);
    // A miss is not retained: assets can become available later in this world.
    // Ref invalidates on deletion, including zero-serial address/index reuse.
    if(value&&objects.size()<2048)objects.emplace(path,Ref(value));
    return value;
}
UObject* asset(const wchar_t* path){
    MeasureOperation timing(Operation::AssetLoad);
    if(auto loaded=find(path))return loaded;
    Call c(find(L"/Script/Engine.Default__KismetSystemLibrary"),L"LoadAsset_Blocking");
    if(!c)return nullptr;auto p=c.field(L"Asset");
    if(!p||!p->IsA<FSoftObjectProperty>()||!p->ImportText_Direct(path,c.value(L"Asset"),nullptr,0,nullptr))return nullptr;
    c.invoke();return c.resultObject();
}
UObject* readObject(FProperty* p,void* base){
    if(!p||!base||!p->IsA<FObjectPropertyBase>())return nullptr;
    return static_cast<FObjectPropertyBase*>(p)->GetObjectPropertyValue(p->ContainerPtrToValuePtr<void>(base));
}
void assignObject(FProperty* p,void* base,UObject* value){
    if(!p||!base||!p->IsA<FObjectProperty>()||p->GetSize()!=sizeof(value))throw std::runtime_error("Expected one hard object reference");
    // UE5.5 no longer maps the legacy SetObjectPropertyValue virtual wrapper.
    // The engine's object-property copy handles TObjectPtr ownership/barriers.
    p->CopyCompleteValue(p->ContainerPtrToValuePtr<void>(base),&value);
}
UObject* object(UObject* p,const wchar_t* name){return readObject(property(p,name),p);}
int64_t integer(FProperty* p,void* base){
    if(!p||!base)throw std::runtime_error("Missing numeric property");auto value=p->ContainerPtrToValuePtr<void>(base);
    if(p->IsA<FBoolProperty>())return static_cast<FBoolProperty*>(p)->GetPropertyValue(value);
    if(p->IsA<FEnumProperty>())return static_cast<FEnumProperty*>(p)->GetUnderlyingProp()->GetSignedIntPropertyValue(value);
    if(p->IsA<FNumericProperty>())return static_cast<FNumericProperty*>(p)->GetSignedIntPropertyValue(value);
    throw std::runtime_error("Expected numeric property");
}
void number(FProperty* p,void* base,double v){
    if(!p)throw std::runtime_error("Missing numeric parameter");auto value=p->ContainerPtrToValuePtr<void>(base);
    if(p->IsA<FBoolProperty>()){static_cast<FBoolProperty*>(p)->SetPropertyValue(value,v!=0);return;}
    if(p->IsA<FEnumProperty>()){static_cast<FEnumProperty*>(p)->GetUnderlyingProp()->SetIntPropertyValue(value,static_cast<int64_t>(v));return;}
    if(p->IsA<FNumericProperty>()){
        auto np=static_cast<FNumericProperty*>(p);if(np->IsFloatingPoint())np->SetFloatingPointPropertyValue(value,v);
        else np->SetIntPropertyValue(value,static_cast<int64_t>(v));return;
    }throw std::runtime_error("Expected numeric parameter");
}
std::wstring text(FProperty* p,void* base){
    if(!p||!base)return {};auto value=p->ContainerPtrToValuePtr<void>(base);
    if(p->IsA<FStrProperty>())return **static_cast<FString*>(value);
    if(p->IsA<FTextProperty>())return static_cast<FText*>(value)->ToString();
    if(p->IsA<FNameProperty>())return static_cast<FName*>(value)->ToString();return {};
}
void assignText(FProperty* p,void* base,const std::wstring& v){
    if(!p)throw std::runtime_error("Missing text parameter");auto value=p->ContainerPtrToValuePtr<void>(base);
    if(p->IsA<FStrProperty>()){*static_cast<FString*>(value)=FString(v.c_str());return;}
    if(p->IsA<FTextProperty>()){
        Call conversion(find(L"/Script/Engine.Default__KismetTextLibrary"),L"Conv_StringToText");
        conversion.str(L"InString",v).invoke();auto result=conversion.fn->GetReturnProperty();
        if(!result||!result->IsA<FTextProperty>()||result->GetSize()!=p->GetSize())throw std::runtime_error("Text layout unavailable");
        p->CopyCompleteValue(value,result->ContainerPtrToValuePtr<void>(conversion.data()));return;
    }
    if(p->IsA<FNameProperty>()){*static_cast<FName*>(value)=FName(v.c_str());return;}
    throw std::runtime_error("Expected text parameter");
}
UObject* construct(const wchar_t* path,UObject* outer){
    auto cls=static_cast<UClass*>(find(path));if(!cls||!outer)return nullptr;
    FStaticConstructObjectParameters args(cls,outer);return UObjectGlobals::StaticConstructObject(args);
}
Call::Call(UObject* o,const wchar_t* name):owner(o),fn(function(o,name)){
    if(!fn){warn(std::wstring(L"Function unavailable: ")+name);return;}auto size=std::max<int>(fn->GetParmsSize(),fn->GetPropertiesSize());if(size<0||size>65536||fn->GetMinAlignment()>16){fn=nullptr;return;}
    storage.resize((size+15)/16+1);fn->InitializeStruct(data());initialized=true;
}
Call::~Call(){if(initialized)fn->DestroyStruct(data());}
FProperty* Call::field(const wchar_t* n){return property(static_cast<UStruct*>(fn),n);}
void* Call::value(const wchar_t* n){auto p=field(n);return p?p->ContainerPtrToValuePtr<void>(data()):nullptr;}
Call& Call::obj(const wchar_t* n,UObject* o){assignObject(field(n),data(),o);return *this;}
Call& Call::num(const wchar_t* n,double v){number(field(n),data(),v);return *this;}
Call& Call::str(const wchar_t* n,const std::wstring& v){assignText(field(n),data(),v);return *this;}
void Call::invoke(){if(!fn)throw std::runtime_error("Required function unavailable");owner->ProcessEvent(fn,data());}
UObject* Call::resultObject(){return readObject(fn?fn->GetReturnProperty():nullptr,data());}
int64_t Call::resultInteger(){return integer(fn?fn->GetReturnProperty():nullptr,data());}
void call(UObject* o,const wchar_t* name){Call c(o,name);c.invoke();}
void setNumber(UObject* o,const wchar_t* f,const wchar_t* p,double n){Call c(o,f);c.num(p,n).invoke();}
void setText(UObject* o,const wchar_t* f,const std::wstring& t){Call c(o,f);if(!c)return;FProperty* p=nullptr;for(auto x:c.fn->ForEachProperty())if(x->GetPropertyFlags()&CPF_Parm){p=x;break;}assignText(p,c.data(),t);c.invoke();}
}

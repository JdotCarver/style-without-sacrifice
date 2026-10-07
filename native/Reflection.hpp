#pragma once
#include <Mod/CppUserModBase.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/Engine/UDataTable.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/FText.hpp>
#include <Unreal/Property/FEnumProperty.hpp>
#include <Unreal/Property/FStrProperty.hpp>
#include <Unreal/Property/FTextProperty.hpp>
#include <Unreal/Property/FSoftObjectProperty.hpp>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace Wardrobe {
using namespace RC::Unreal;
struct ObjectLife {std::atomic_bool alive{true};UObject* address{};};
struct Ref {
    UObject* address{};int index=-1,serial{};std::shared_ptr<ObjectLife> life;
    Ref()=default;explicit Ref(UObject*);
    UObject* get()const;
    explicit operator bool()const{return get()!=nullptr;}
    bool matches(UObject* p)const{return p&&address==p&&get()==p;}
};
void initializeReferences();void shutdownReferences();void clearReflection();
FProperty* property(UStruct*,const std::wstring&);
FProperty* property(UObject*,const std::wstring&);
UFunction* function(UObject*,const std::wstring&);
UObject* find(const wchar_t*);UObject* object(UObject*,const wchar_t*);
UObject* asset(const wchar_t*);
UObject* construct(const wchar_t*,UObject*);
int64_t integer(FProperty*,void*);void number(FProperty*,void*,double);
void assignObject(FProperty*,void*,UObject*);
UObject* readObject(FProperty*,void*);
std::wstring text(FProperty*,void*);void assignText(FProperty*,void*,const std::wstring&);
struct Call {
    struct alignas(16) Block {unsigned char bytes[16];};
    UObject* owner{};UFunction* fn{};std::vector<Block> storage;bool initialized{};
    Call(UObject*,const wchar_t*);~Call();
    Call(const Call&)=delete;Call& operator=(const Call&)=delete;
    explicit operator bool()const{return fn!=nullptr;}
    void* data(){return storage.data();}
    FProperty* field(const wchar_t*);
    void* value(const wchar_t*);
    Call& obj(const wchar_t*,UObject*);Call& num(const wchar_t*,double);Call& str(const wchar_t*,const std::wstring&);
    void invoke();UObject* resultObject();int64_t resultInteger();
};
void call(UObject*,const wchar_t*);
void setNumber(UObject*,const wchar_t*,const wchar_t*,double);
void setText(UObject*,const wchar_t*,const std::wstring&);
void warn(const std::wstring&);void trace(const std::wstring&);
void failure(const wchar_t*,const std::exception&);
extern bool logging;
extern std::atomic_int logLevel;
}

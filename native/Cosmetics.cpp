#include "Runtime.hpp"
#include "NativeContract.hpp"
#include <Windows.h>
#include <MinHook.h>
#include <array>
#include <cstring>
#include <map>
#include <chrono>

namespace Wardrobe {
namespace {
using RowGetter=void*(*)(UObject*,UObject*);
RowGetter originalRow{};void* rowTarget{};DWORD threadId{};
Ref subsystem,clothingTable,weaponTable,rowType,weaponType;
FProperty* slotProperty{};std::array<int,4> slotValues{1,2,3,4};
struct OwnedRow {
    Ref type;std::vector<uint64_t> bytes;
    OwnedRow(UScriptStruct* t,const void* data):type(t),bytes((t->GetPropertiesSize()+7)/8+1){t->InitializeStruct(bytes.data());if(data)t->CopyScriptStruct(bytes.data(),data);}
    ~OwnedRow(){if(auto t=static_cast<UScriptStruct*>(type.get()))t->DestroyStruct(bytes.data());}
};
std::array<std::array<std::unique_ptr<OwnedRow>,4>,2> overrides;
std::array<Ref,8> weapons;
std::array<Ref,2> weaponMesh,scabbardMesh;
unsigned tablePhase{},tableCursor{};int expectedRows{};uint64_t lastLoadoutFrame=~0ull;
bool catalogStarted{},insideRefresh{},started{};
uint64_t preparedRevision=~0ull;
bool inventoryDirty=true;
struct MeshEdit {Ref component,original,installed;bool visible{},appliedVisible{},changedMesh{},changedVisibility{};};
std::array<MeshEdit,24> meshEdits;
void restoreMeshes(){
 for(auto& edit:meshEdits){if(auto component=edit.component.get()){
  if(edit.changedMesh&&object(component,L"StaticMesh")==edit.installed.address&&(!edit.installed.address||edit.installed)&&(!edit.original.address||edit.original)){Call c(component,L"SetStaticMesh");if(c)c.obj(L"NewMesh",edit.original.get()).invoke();}
  if(edit.changedVisibility){Call get(component,L"IsVisible");if(get){get.invoke();if((get.resultInteger()!=0)==edit.appliedVisible){Call set(component,L"SetVisibility");set.num(L"bNewVisibility",edit.visible).invoke();}}}
 }edit={};}
}
void editMesh(UObject* component,UObject* replacement,bool changeMesh,bool visible){
 if(!component)return;MeshEdit* entry=nullptr;for(auto& e:meshEdits)if(!e.component){entry=&e;break;}if(!entry)return;
 entry->component=Ref(component);entry->original=Ref(object(component,L"StaticMesh"));entry->installed=Ref(replacement);
 Call get(component,L"IsVisible");if(get){get.invoke();entry->visible=get.resultInteger()!=0;entry->changedVisibility=entry->visible!=visible;entry->appliedVisible=visible;if(entry->changedVisibility){Call set(component,L"SetVisibility");set.num(L"bNewVisibility",visible).invoke();}}
 if(changeMesh&&replacement!=entry->original.get()){Call set(component,L"SetStaticMesh");if(set){set.obj(L"NewMesh",replacement).invoke();entry->changedMesh=true;}}
}
std::map<std::wstring,std::wstring> itemRows;
std::wstring cleanName(std::wstring name){
    for(auto prefix:{L"ITM_",L"Weapon_",L"Armor_",L"Clothing_"})if(name.starts_with(prefix))name.erase(0,wcslen(prefix));
    std::replace(name.begin(),name.end(),L'_',L' ');return name;
}
int slotFor(void* row){if(!slotProperty||!row)return -1;auto v=integer(slotProperty,row);for(int i=0;i<4;++i)if(v==slotValues[i])return i;return -1;}
UDataTable* table(bool weapon){return static_cast<UDataTable*>((weapon?weaponTable:clothingTable).get());}
void* lookup(bool weapon,const std::wstring& key){auto t=table(weapon);if(!t||key.empty())return nullptr;auto p=t->GetRowMap().Find(FName(key.c_str(),FNAME_Find));return p?*p:nullptr;}
unsigned loadout(){auto inv=runtime.inventory.get();if(!inv)return runtime.activeSet;Call c(inv,L"GetActiveLoadoutIndex");if(!c)return runtime.activeSet;c.invoke();return static_cast<unsigned>(std::clamp<int64_t>(c.resultInteger(),0,1));}
bool targetContext(UObject* context){return runtime.appearance.matches(context)||runtime.player.matches(context)||runtime.doll.matches(context)||runtime.dollAppearance.matches(context);}
void* clothingRow(UObject* item,UObject* context){
    auto result=originalRow(item,context);
    if(!started||GetCurrentThreadId()!=threadId||!runtime.settings.enabled||!result||!targetContext(context))return result;
    try{
        if(lastLoadoutFrame!=runtime.frame){lastLoadoutFrame=runtime.frame;auto set=loadout();if(set!=runtime.activeSet){runtime.activeSet=set;if(!runtime.menuOpen)runtime.model.switchSet(set);runtime.refreshWork.request(runtime.now);}}
        int slot=slotFor(result);if(slot<0)return result;
        unsigned set=(runtime.doll.matches(context)||runtime.dollAppearance.matches(context))?runtime.model.displayedSet:runtime.activeSet;
        auto& replacement=overrides[set][slot];
        if(replacement&&replacement->type.get()){if(logging)++runtime.nativeOverrides;return replacement->bytes.data();}
    }catch(...){warn(L"Clothing layout unavailable; keeping the equipped appearance.");}
    return result;
}
UObject* loadAsset(FProperty* source,void* data){
    if(!source||!data)return nullptr;
    if(source->IsA<FObjectPropertyBase>()&&!source->IsA<FSoftObjectProperty>())return readObject(source,data);
    Call c(find(L"/Script/Engine.Default__KismetSystemLibrary"),L"LoadAsset_Blocking");if(!c)return nullptr;auto target=c.field(L"Asset");
    if(!target||target->GetSize()!=source->GetSize()||!source->IsA<FSoftObjectProperty>())return nullptr;
    target->CopyCompleteValue(c.value(L"Asset"),source->ContainerPtrToValuePtr<void>(data));c.invoke();return c.resultObject();
}
void visibility(UObject* mesh,bool visible){if(!mesh)return;Call c(mesh,L"SetVisibility");if(c){c.num(L"bNewVisibility",visible);if(c.field(L"bPropagateToChildren"))c.num(L"bPropagateToChildren",1);c.invoke();}}
void staticMesh(UObject* component,UObject* mesh){if(!component||!mesh)return;Call c(component,L"SetStaticMesh");if(c)c.obj(L"NewMesh",mesh).invoke();}
void weaponAppearance(UObject* actor){
    if(!actor)return;auto owner=object(actor,L"Owner");if(!runtime.player.matches(owner)&&!runtime.doll.matches(owner))return;
    auto mesh=object(actor,L"BaseMesh");if(!mesh)return;unsigned set=runtime.doll.matches(owner)?runtime.model.displayedSet:runtime.activeSet;auto& c=runtime.model.sets[set][4];
    // Drawn weapons remain visible when Hidden, matching the original Wardrobe.
    if(runtime.settings.enabled&&c.mode==Choice::Mode::Look&&weaponMesh[set])editMesh(mesh,weaponMesh[set].get(),true,true);
}
void prepareRows(){
    bool assetsReady=true;for(unsigned set=0;set<2;++set)if(runtime.model.sets[set][4].mode==Choice::Mode::Look&&!weaponMesh[set])assetsReady=false;
    if(preparedRevision==runtime.model.revision&&assetsReady)return;
    auto type=static_cast<UScriptStruct*>(rowType.get());if(!type)return;
    for(unsigned set=0;set<2;++set)for(unsigned slot=0;slot<4;++slot){
        auto& choice=runtime.model.sets[set][slot];auto& snapshot=overrides[set][slot];snapshot.reset();
        if(choice.mode==Choice::Mode::Original)continue;
        auto row=choice.mode==Choice::Mode::Look?lookup(false,choice.row):nullptr;
        if(choice.mode==Choice::Mode::Look&&(!row||slotFor(row)!=static_cast<int>(slot))){warn(L"A saved garment is unavailable; keeping the equipped look.");continue;}
        snapshot=std::make_unique<OwnedRow>(type,row);number(slotProperty,snapshot->bytes.data(),slotValues[slot]);
    }
    preparedRevision=runtime.model.revision;
    auto wt=static_cast<UScriptStruct*>(weaponType.get());
    for(unsigned set=0;set<2;++set){weaponMesh[set]={};scabbardMesh[set]={};auto& c=runtime.model.sets[set][4];
        if(wt&&c.mode==Choice::Mode::Look)if(auto row=lookup(true,c.row)){
            weaponMesh[set]=Ref(loadAsset(property(wt,L"WeaponMesh"),row));scabbardMesh[set]=Ref(loadAsset(property(wt,L"ScabbardMesh"),row));
        }
    }
}
bool tables(UObject* pawn){
    Call sub(find(L"/Script/Engine.Default__SubsystemBlueprintLibrary"),L"GetGameInstanceSubsystem");if(!sub)return false;
    sub.obj(L"ContextObject",pawn).obj(L"Class",find(L"/Script/DogwoodInventory.AppearanceSubsystem")).invoke();auto system=sub.resultObject();if(!system)return false;
    auto ct=object(system,L"LoadedAppearanceUnitTable");
    auto wt=object(system,L"LoadedWeaponAppearances");
    if(!wt)wt=object(find(L"/Script/DogwoodInventory.Default__DogwoodInventorySettings"),L"LoadedWeaponAppearances");
    if(!wt)wt=find(L"/Game/_Dawnwalker/Inventory/Items/DT_WeaponAppearances.DT_WeaponAppearances");
    if(!ct||!ct->IsA<UDataTable>())return false;
    auto type=static_cast<UDataTable*>(ct)->GetRowStruct().Get();
    if(!type||type->GetName()!=L"AppearanceClothingUnitRow"||type->GetPropertiesSize()!=120)return false;
    auto slot=property(type,L"Slot"),mesh=property(type,L"Mesh");if(!slot||!mesh||!mesh->IsA<FStructProperty>())return false;
    UEnum* enumeration=nullptr;
    if(slot->IsA<FEnumProperty>())enumeration=static_cast<FEnumProperty*>(slot)->GetEnum();
    else if(slot->IsA<FByteProperty>())enumeration=static_cast<FByteProperty*>(slot)->GetEnum();
    if(!enumeration)return false;
    std::array<bool,4> found{};
    for(auto pair:enumeration->ForEachName()){auto n=pair.Key.ToString();for(unsigned i=0;i<4;++i)if(n==slotNames[i]||n.ends_with(std::wstring(L"::")+slotNames[i])){slotValues[i]=static_cast<int>(pair.Value);found[i]=true;}}
    if(!std::all_of(found.begin(),found.end(),[](bool b){return b;}))return false;
    subsystem=Ref(system);clothingTable=Ref(ct);rowType=Ref(type);slotProperty=slot;
    if(wt&&wt->IsA<UDataTable>()){weaponTable=Ref(wt);weaponType=Ref(static_cast<UDataTable*>(wt)->GetRowStruct().Get());}
    return true;
}
}
bool startCosmetics(){
    threadId=GetCurrentThreadId();std::wstring error;
    rowTarget=NativeContract::resolve(error);
    if(!rowTarget){warn(L"Clothing overrides unavailable: "+error);return false;}
    auto initialized=MH_Initialize();if(initialized!=MH_OK&&initialized!=MH_ERROR_ALREADY_INITIALIZED){warn(L"Native hook service unavailable.");return false;}
    if(MH_CreateHook(rowTarget,reinterpret_cast<void*>(clothingRow),reinterpret_cast<void**>(&originalRow))!=MH_OK){warn(L"Clothing lookup is already modified or cannot be hooked.");rowTarget=nullptr;return false;}
    if(MH_EnableHook(rowTarget)!=MH_OK){MH_RemoveHook(rowTarget);rowTarget=nullptr;return false;}
    started=true;return true;
}
void stopCosmetics(){started=false;if(rowTarget){MH_DisableHook(rowTarget);MH_RemoveHook(rowTarget);rowTarget=nullptr;}resetCosmetics();}
void resetCosmetics(){meshEdits={};preparedRevision=~0ull;inventoryDirty=true;for(auto& set:overrides)for(auto& row:set)row.reset();subsystem={};clothingTable={};weaponTable={};rowType={};weaponType={};slotProperty=nullptr;weapons={};weaponMesh={};scabbardMesh={};catalogStarted=false;runtime.catalogReady=false;runtime.looks.clear();itemRows.clear();lastLoadoutFrame=~0ull;}
bool attach(UObject* pawn){
    if(!pawn)return false;auto controller=object(pawn,L"Controller");if(!controller||!object(controller,L"Player"))return false;
    auto inv=object(pawn,L"InventoryComponent"),app=object(pawn,L"AppearanceComponent");if(!inv||!app)return false;
    if(!runtime.player.matches(pawn)){resetCosmetics();runtime.player=Ref(pawn);runtime.controller=Ref(controller);runtime.inventory=Ref(inv);runtime.appearance=Ref(app);runtime.doll={};runtime.dollAppearance={};}
    if(!tables(pawn))return false;beginCatalog();runtime.activeSet=loadout();runtime.model.switchSet(runtime.activeSet);prepareRows();runtime.refreshWork.request(runtime.now);if(logging)trace(L"Attached player: "+pawn->GetName());return true;
}
void beginCatalog(){if(catalogStarted||!clothingTable)return;catalogStarted=true;tablePhase=tableCursor=0;expectedRows=table(false)->GetRowMap().Num();runtime.looks.clear();itemRows.clear();runtime.catalogReady=false;}
bool stepCatalog(){
    if(!catalogStarted||runtime.catalogReady)return true;
    auto t=table(tablePhase==1);if(!t){if(tablePhase==0)return false;runtime.catalogReady=true;return true;}
    auto type=static_cast<UScriptStruct*>((tablePhase==1?weaponType:rowType).get());if(!type)return false;
    auto& rows=t->GetRowMap();if(rows.Num()>16384||rows.GetMaxIndex()>32768){warn(L"Appearance catalog exceeds the supported safety limit.");catalogStarted=false;return true;}
    if(rows.Num()!=expectedRows){catalogStarted=false;beginCatalog();return false;}
    unsigned count=0;auto start=std::chrono::steady_clock::now();
    while(tableCursor<static_cast<unsigned>(rows.GetMaxIndex())&&count++<16){
        auto id=FSetElementId::FromInteger(tableCursor++);if(!rows.IsValidId(id))continue;const auto& pair=rows.Get(id);auto row=pair.Value;if(!row)continue;
        int slot=tablePhase?4:slotFor(row);if(slot<0)continue;
        Look look{static_cast<Slot>(slot),pair.Key.ToString(),{}, {}};look.label=cleanName(look.row);
        auto item=readObject(property(type,L"Item"),row);
        if(!item&&tablePhase==1){auto path=L"/Game/_Dawnwalker/Inventory/Items/"+look.row+L"."+look.row;item=find(path.c_str());}
        if(item){look.itemPath=item->GetPathName();auto name=text(property(item,L"ItemName"),item);if(!name.empty())look.label=name;itemRows[look.itemPath]=look.row;}
        runtime.looks.push_back(std::move(look));
        if(std::chrono::steady_clock::now()-start>std::chrono::microseconds(500))break;
    }
    if(logging)++runtime.catalogSteps;
    if(tableCursor>=static_cast<unsigned>(rows.GetMaxIndex())){
        if(tablePhase==0&&weaponTable){tablePhase=1;tableCursor=0;expectedRows=table(true)->GetRowMap().Num();}
        else {runtime.catalogReady=true;learnInventory();std::stable_sort(runtime.looks.begin(),runtime.looks.end(),[](auto& a,auto& b){return a.label<b.label;});menuRedraw();}
    }
    return runtime.catalogReady;
}
void inventoryChanged(){inventoryDirty=true;runtime.refreshWork.request(runtime.now);}
bool catalogPending(){return catalogStarted&&!runtime.catalogReady;}
void learnInventory(){
    auto inv=runtime.inventory.get();if(!inv||!runtime.catalogReady)return;Call c(inv,L"GetCurrentItems");if(!c)return;c.invoke();
    auto p=c.fn->GetReturnProperty();if(!p||!p->IsA<FArrayProperty>())return;auto arr=static_cast<FArrayProperty*>(p);auto inner=arr->GetInner();if(!inner->IsA<FStructProperty>())return;
    auto st=static_cast<FStructProperty*>(inner)->GetStruct();auto asset=property(st,L"ItemDataAsset");if(!asset)return;
    FScriptArrayHelper helper(arr,p->ContainerPtrToValuePtr<void>(c.data()));if(helper.Num()>16384)return;
    bool learned=false;
    for(int i=0;i<helper.Num();++i)if(auto item=readObject(asset,helper.GetRawPtr(i))){
        auto it=itemRows.find(item->GetPathName());auto name=item->GetName();
        if(it!=itemRows.end())learned|=runtime.model.learn(it->second);
        else if(lookup(true,name))learned|=runtime.model.learn(name);
    }
    if(learned){runtime.dirty=true;menuRedraw();}
}
bool refresh(){
    if(insideRefresh)return true;auto app=runtime.appearance.get();if(!app)return false;
    insideRefresh=true;
    try{
        runtime.activeSet=loadout();restoreMeshes();prepareRows();
        // Rebuild cosmetics through the reflected appearance API. Equipment and
        // active loadout are never changed to refresh a preview.
        Call c(app,L"OnInventoryContentsChanged");if(!c||c.fn->GetParmsSize()!=0){insideRefresh=false;warn(L"Appearance refresh signature unavailable.");return true;}c.invoke();
        if(auto doll=runtime.dollAppearance.get()){Call preview(doll,L"OnInventoryContentsChanged");if(preview&&preview.fn->GetParmsSize()==0)preview.invoke();}
        for(auto owner:{app,runtime.player.get(),runtime.doll.get()})if(owner&&runtime.settings.enabled){
            bool doll=runtime.doll.matches(owner);unsigned set=doll?runtime.model.displayedSet:runtime.activeSet;auto& choice=runtime.model.sets[set][4];
            auto sheathed=object(owner,doll?L"WeaponMesh":L"SheathedWeaponMesh");auto scabbard=object(owner,L"Scabbard");
            if(choice.mode==Choice::Mode::Hidden){editMesh(sheathed,nullptr,false,false);editMesh(scabbard,nullptr,false,false);}
            else if(choice.mode==Choice::Mode::Look&&weaponMesh[set]){editMesh(sheathed,weaponMesh[set].get(),true,true);editMesh(scabbard,scabbardMesh[set].get(),true,bool(scabbardMesh[set]));}
        }
        for(auto& w:weapons)if(auto actor=w.get())weaponAppearance(actor);
        if(inventoryDirty){learnInventory();inventoryDirty=false;}if(logging)++runtime.refreshes;insideRefresh=false;return true;
    }catch(...){insideRefresh=false;warn(L"Appearance refresh could not complete; normal equipment remains intact.");return false;}
}
void selectLook(Slot slot,const Choice& c){if(runtime.model.choose(slot,c)){runtime.dirty=true;runtime.refreshWork.request(runtime.now);menuRedraw();if(logging)trace(L"Look selected: set="+std::to_wstring(runtime.model.displayedSet)+L", slot="+slotNames[static_cast<unsigned>(slot)]+L", mode="+std::to_wstring(static_cast<int>(c.mode))+L", row="+c.row);}}
void setDoll(UObject* p){runtime.doll=Ref(p);runtime.dollAppearance=Ref(object(p,L"AppearanceComponent"));runtime.refreshWork.request(runtime.now);}
void observeWeapon(UObject* p){if(!p||!object(p,L"WeaponDataAsset"))return;auto owner=object(p,L"Owner");if(!runtime.player.matches(owner)&&!runtime.doll.matches(owner))return;for(auto& r:weapons)if(r.matches(p))return;for(auto& r:weapons)if(!r){r=Ref(p);runtime.refreshWork.request(runtime.now);return;}}
}

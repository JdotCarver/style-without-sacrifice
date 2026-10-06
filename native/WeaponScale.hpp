#pragma once
#include "Reflection.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <optional>

namespace Wardrobe {
using MeshScale=std::array<double,3>;
inline bool scaleField(FProperty* field){
    if(!field||!field->IsA<FStructProperty>()||field->GetSize()!=sizeof(MeshScale))return false;
    auto type=static_cast<FStructProperty*>(field)->GetStruct();
    if(!type||type->GetName()!=L"Vector"||type->GetPropertiesSize()!=sizeof(MeshScale))return false;
    const wchar_t* names[]={L"X",L"Y",L"Z"};
    for(unsigned i=0;i<3;++i){auto p=property(type,names[i]);
        if(!p||!p->IsA<FDoubleProperty>()||p->GetSize()!=sizeof(double)||static_cast<const FProperty*>(p)->GetOffset_Internal()!=i*sizeof(double))return false;
    }
    return true;
}
inline std::optional<MeshScale> readScale(FProperty* field,void* owner){
    if(!owner||!scaleField(field))return {};
    MeshScale value;std::memcpy(value.data(),field->ContainerPtrToValuePtr<void>(owner),sizeof(value));
    for(auto v:value)if(!std::isfinite(v))return {};
    return value;
}
inline bool writeScale(UObject* component,const MeshScale& value){
    Call set(component,L"SetRelativeScale3D");
    if(!set||!scaleField(set.field(L"NewScale3D"))||set.fn->GetParmsSize()!=sizeof(MeshScale)||
       static_cast<const FProperty*>(set.field(L"NewScale3D"))->GetOffset_Internal()!=0)return false;
    std::memcpy(set.value(L"NewScale3D"),value.data(),sizeof(value));set.invoke();
    return readScale(property(component,L"RelativeScale3D"),component)==value;
}
inline std::optional<MeshScale> weaponScaleFailure(const wchar_t* reason,const std::wstring& row){
    warn(std::wstring(L"Selected weapon sizing unavailable: ")+reason+L" Keeping the equipped weapon appearance.");
    if(logging)trace(L"Weapon sizing failed: row="+row+L", reason="+reason);
    return {};
}
inline std::optional<MeshScale> selectedWeaponScale(const std::wstring& row,const wchar_t* templateClass=nullptr,Ref* templateMesh=nullptr){
    // Resolve the chosen appearance's item, never the equipped weapon. All
    // appearance-table keys are item asset names. Only a changed choice
    // reaches this path; missing assets are not polled in the background.
    // These stock rows share the exact weapon mesh with the named item, but
    // the demo has no item asset and the sacrificial knife has no class reference.
    // Resolve real defaults instead of guessing a scale or rejecting the look.
    UObject* cls=nullptr;
    if(templateClass)cls=asset(templateClass);
    else {
        const auto& sizingRow=row==L"ITM_Weapon_SwordGreatMaster6aDemoOnly"?std::wstring(L"ITM_Weapon_SwordGreatMaster6a"):
            row==L"ITM_Weapon_SacrificialKnife"?std::wstring(L"ITM_Weapon_LeonicaKnife"):row;
        auto path=L"/Game/_Dawnwalker/Inventory/Items/"+sizingRow+L"."+sizingRow;
        auto item=asset(path.c_str());
        if(!item){
            // Only audited catalog-only rows use unit scale. A failed load for a
            // normal item must not silently shrink a large weapon to unit size.
            for(auto name:{L"ITM_Weapon_AxeTest",L"ITM_Weapon_MaceTest",L"ITM_Weapon_VampireClawsTest",
                L"ITM_Weapon_AxeTest2",L"ITM_Weapon_BoardTest",L"ITM_Weapon_HammerTest",L"ITM_Weapon_PickaxeTest",
                L"ITM_Weapon_SwordLongCommon2DemoOnly",L"ITM_Weapon_SwordLongSuperior2DemoOnly",
                L"ITM_Weapon_SwordLongSuperior7DemoOnly",L"ITM_Weapon_DawnwalkerSword",
                L"ITM_Weapon_AxeShortCommon1",L"ITM_Weapon_AxeShortSuperior1",L"ITM_Weapon_AxeShortMaster1"}){
                if(row==name){
                    if(logging)trace(L"Catalog-only weapon appearance uses unit scale: "+row);
                    return MeshScale{1,1,1};
                }
            }
            return weaponScaleFailure(L"Selected weapon item defaults could not be loaded.",row);
        }
        auto blueprint=property(item,L"WeaponBlueprint");
        // Class loading may dispatch engine callbacks. Copy the asset reference
        // into owned parameters before loading; no borrowed item field survives it.
        if(blueprint&&blueprint->IsA<FSoftClassProperty>()){
            Call load(find(L"/Script/Engine.Default__KismetSystemLibrary"),L"LoadClassAsset_Blocking");
            auto input=load.field(L"AssetClass"),result=load?load.fn->GetReturnProperty():nullptr;
            if(!input||!input->IsA<FSoftClassProperty>()||input->GetSize()!=blueprint->GetSize()||
               !result||!result->IsA<FClassProperty>()||result->GetSize()!=sizeof(void*)||
               static_cast<const FProperty*>(input)->GetOffset_Internal()!=0||
               static_cast<const FProperty*>(result)->GetOffset_Internal()!=input->GetSize()||
               load.fn->GetParmsSize()!=input->GetSize()+sizeof(void*))
                return weaponScaleFailure(L"LoadClassAsset_Blocking has no compatible AssetClass parameter.",row);
            input->CopyCompleteValue(load.value(L"AssetClass"),blueprint->ContainerPtrToValuePtr<void>(item));
            load.invoke();cls=load.resultObject();
        }else if(blueprint&&blueprint->IsA<FClassProperty>()&&blueprint->GetSize()==sizeof(void*)){
            cls=readObject(blueprint,item);
        }else return weaponScaleFailure(L"WeaponBlueprint is not a supported class reference.",row);
    }
    if(!cls||!cls->IsA<UClass>())return weaponScaleFailure(L"WeaponBlueprint did not load a class.",row);
    auto defaults=static_cast<UClass*>(cls)->GetClassDefaultObject().Get();
    if(!defaults||!defaults->IsA(static_cast<UClass*>(cls)))return weaponScaleFailure(L"Weapon class defaults are unavailable.",row);
    // GetSheathedWeaponScale contains the game's stowage reduction (0.8 for
    // greatswords), not the selected mesh's authored size (1.2). Use the class's
    // BaseMesh template for both drawn and sheathed appearances. Never modify it.
    auto meshField=property(defaults,L"BaseMesh");
    if(!meshField||!meshField->IsA<FObjectProperty>()||meshField->GetSize()!=sizeof(void*))
        return weaponScaleFailure(L"Weapon class BaseMesh is not an object reference.",row);
    auto mesh=readObject(meshField,defaults);
    if(!mesh)return weaponScaleFailure(L"Weapon class BaseMesh defaults are unavailable.",row);
    // A corrected debug appearance can use the same template for both mesh
    // and size. Do not fall back to its unrelated appearance-table prop.
    if(templateMesh){
        auto field=property(mesh,L"StaticMesh");
        if(!field||!field->IsA<FObjectProperty>()||field->GetSize()!=sizeof(void*))
            return weaponScaleFailure(L"Weapon template StaticMesh is not an object reference.",row);
        *templateMesh=Ref(readObject(field,mesh));
        if(!*templateMesh)return weaponScaleFailure(L"Weapon template has no static mesh.",row);
    }
    auto scale=readScale(property(mesh,L"RelativeScale3D"),mesh);
    if(!scale)return weaponScaleFailure(L"Weapon class BaseMesh has an incompatible or invalid scale.",row);
    for(auto v:*scale)if(v<=0)return weaponScaleFailure(L"Weapon class BaseMesh has a nonpositive scale.",row);
    return scale;
}
}

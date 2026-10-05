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
inline std::optional<MeshScale> selectedWeaponScale(const std::wstring& row){
    // Resolve the chosen appearance's item, never the equipped weapon. All
    // appearance-table keys are item asset names. Only a changed choice
    // reaches this path; missing assets are not polled in the background.
    auto path=L"/Game/_Dawnwalker/Inventory/Items/"+row+L"."+row;
    auto item=asset(path.c_str());
    if(!item){
        // Catalog-only test/demo meshes have no item or type-specific sizing.
        // Show their authored size instead of inheriting the equipped item's.
        if(logging)trace(L"Weapon appearance has no item defaults; using authored mesh scale: "+row);
        return MeshScale{1,1,1};
    }
    auto type=property(item,L"WeaponType"),blueprint=property(item,L"WeaponBlueprint");
    if(!type||!type->IsA<FEnumProperty>()||type->GetSize()!=1||!blueprint||!blueprint->IsA<FObjectProperty>()||blueprint->GetSize()!=sizeof(void*))return {};
    auto cls=readObject(blueprint,item);if(!cls||!cls->IsA<UClass>())return {};
    auto defaults=static_cast<UClass*>(cls)->GetClassDefaultObject().Get();
    if(!defaults||!defaults->IsA(static_cast<UClass*>(cls)))return {};
    Call get(defaults,L"GetSheathedWeaponScale");if(!get)return {};
    auto input=get.field(L"WeaponType"),result=get.fn->GetReturnProperty();
    if(!input||!input->IsA<FEnumProperty>()||input->GetSize()!=1||
       static_cast<FEnumProperty*>(input)->GetEnum()!=static_cast<FEnumProperty*>(type)->GetEnum()||
       !scaleField(result)||get.fn->GetParmsSize()!=32||
       static_cast<const FProperty*>(input)->GetOffset_Internal()!=0||static_cast<const FProperty*>(result)->GetOffset_Internal()!=8)return {};
    get.num(L"WeaponType",integer(type,item)).invoke();
    auto scale=readScale(result,get.data());
    if(scale)for(auto v:*scale)if(v<=0)return {};
    return scale;
}
}

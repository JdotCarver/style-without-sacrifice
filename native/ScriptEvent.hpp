#pragma once
#include "Reflection.hpp"
#include <Unreal/FFrame.hpp>

namespace Wardrobe {
// Blueprint reference parameters (including const inputs) use OutParms.
// Locals can contain only a default value at that property's offset.
inline void* scriptParameter(FProperty* p,void* locals,FFrame* frame){
    if(!p)return nullptr;
    if(p->GetPropertyFlags()&CPF_OutParm)return frame?FindOutParamValueAddress(*frame,p):nullptr;
    return locals?p->ContainerPtrToValuePtr<void>(locals):nullptr;
}
// Unreal may execute a parameterless Blueprint event directly at an entry
// inside its event graph. Bind both routes from reflected function metadata.
struct ScriptEvent {
    Ref event,graph;FProperty* entry{};int offset=-1;
    void bind(UObject* owner,const wchar_t* name){
        *this={};auto fn=function(owner,name);if(!fn)return;event=Ref(fn);
        if(fn->GetNumParms()!=0)return;
        auto target=fn->GetEventGraphFunction();auto at=fn->GetEventGraphCallOffset();
        if(!target||at<0||at>=target->GetScript().Num())return;
        auto p=property(static_cast<UStruct*>(target),L"EntryPoint");
        if(!p||!p->IsA<FIntProperty>()||p->GetSize()!=sizeof(int32_t)||(p->GetPropertyFlags()&CPF_Parm)==0)return;
        graph=Ref(target);entry=p;offset=at;
    }
    bool matches(UFunction* fn,void* params)const{
        if(event.matches(fn))return true;
        return params&&entry&&graph.matches(fn)&&integer(entry,params)==offset;
    }
};
}

#pragma once
// Validate the ABI and instance that this mod intercepts. The original game
// retains ownership of event construction, dispatch, device mapping and cleanup.
// Their implementation, function lengths and game version are not allowlists.
#include "SheathContract.hpp"
namespace Wardrobe::ControllerContract {
using namespace SheathContract;
inline constexpr std::array<unsigned char,78> analogEntryCode={0x48,0x8b,0xc4,0x48,0x89,0x58,0x10,0x48,0x89,0x70,0x18,0x48,0x89,0x78,0x20,0x55,0x48,0x8d,0x68,0xa9,0x48,0x81,0xec,0xf0,0x0,0x0,0x0,0x48,0x8b,0x5,0xca,0xa2,0xad,0x5,0x48,0x33,0xc4,0x48,0x89,0x45,0x47,0x48,0x8d,0xb1,0xb8,0xfe,0xff,0xff,0x48,0x89,0x55,0xbf,0xf,0x57,0xc0,0x48,0x8d,0x55,0x3f,0x48,0x8b,0xce,0x45,0x8b,0xc1,0x41,0x8b,0xd9,0xf3,0xf,0x7f,0x45,0xc7,0xe8,0x66,0xea,0xff,0xff};
inline constexpr std::array<unsigned char,78> analogEntryMask={0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x0,0x0,0x0,0x0,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x0,0x0,0x0,0x0};
inline constexpr std::array<unsigned char,78> buttonEntryCode={0x48,0x8b,0xc4,0x48,0x89,0x58,0x10,0x48,0x89,0x70,0x18,0x48,0x89,0x78,0x20,0x55,0x48,0x8d,0x68,0xa9,0x48,0x81,0xec,0xd0,0x0,0x0,0x0,0x48,0x8b,0x5,0x42,0xa1,0xad,0x5,0x48,0x33,0xc4,0x48,0x89,0x45,0x47,0x48,0x8d,0xb1,0xb8,0xfe,0xff,0xff,0x48,0x89,0x55,0xcf,0xf,0x57,0xc0,0x48,0x8d,0x55,0x3f,0x48,0x8b,0xce,0x45,0x8b,0xc1,0x41,0x8b,0xd9,0xf3,0xf,0x7f,0x45,0xd7,0xe8,0xde,0xe8,0xff,0xff};
inline constexpr std::array<unsigned char,78> buttonEntryMask={0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x0,0x0,0x0,0x0,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x0,0x0,0x0,0x0};
inline constexpr std::array<unsigned char,46> analogValueCode={0xf3,0xf,0x10,0x45,0x7f,0x48,0x8d,0x4d,0xb7,0x48,0x89,0x4c,0x24,0x40,0x48,0x8d,0x55,0xd7,0x48,0x8d,0x4d,0xef,0xf3,0xf,0x11,0x44,0x24,0x38,0x44,0x8b,0xcb,0x4c,0x8b,0xc0,0xe8,0x28,0x59,0xff,0xff,0x48,0x8d,0x55,0xef,0x48,0x8b,0xce};
inline constexpr std::array<unsigned char,46> analogValueMask={0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x0,0x0,0x0,0x0,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
inline constexpr std::array<unsigned char,39> buttonValueCode={0x48,0x8d,0x45,0xc7,0x44,0x8b,0xcb,0x48,0x89,0x44,0x24,0x38,0x48,0x8d,0x55,0xe7,0x8a,0x45,0x7f,0x48,0x8d,0x4d,0xff,0x88,0x44,0x24,0x20,0xe8,0x4c,0x5a,0xff,0xff,0x48,0x8d,0x55,0xff,0x48,0x8b,0xce};
inline constexpr std::array<unsigned char,39> buttonValueMask={0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x0,0x0,0x0,0x0,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
inline constexpr std::array<unsigned char,25> focusCode={0x33,0xc0,0x48,0x39,0x42,0x20,0xf,0x95,0xc0,0x48,0x1,0x42,0x20,0x48,0x8b,0xd,0xd4,0x97,0x37,0x5,0xe9,0x97,0x43,0xaa,0xfc};
inline constexpr std::array<unsigned char,25> focusMask={0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x0,0x0,0x0,0x0,0xff,0x0,0x0,0x0,0x0};

struct Binding {std::array<void*,3> targets{};void** application{};};
inline bool readable(const void* p,size_t n){
    auto at=reinterpret_cast<uintptr_t>(p);
    if(!at||!n||at>UINTPTR_MAX-n)return false;
    const auto end=at+n;
    while(at<end){
        MEMORY_BASIC_INFORMATION m{};
        if(!VirtualQuery(reinterpret_cast<void*>(at),&m,sizeof(m))||m.State!=MEM_COMMIT||
           (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))||!(m.Protect&0xee))return false;
        auto begin=reinterpret_cast<uintptr_t>(m.BaseAddress);
        if(at<begin||at-begin>=m.RegionSize)return false;
        at+=std::min<size_t>(end-at,m.RegionSize-(at-begin));
    }
    return true;
}
inline bool imageRange(const void* p,size_t n,unsigned char* base,size_t size){
    auto at=reinterpret_cast<uintptr_t>(p),begin=reinterpret_cast<uintptr_t>(base);
    return n<=size&&at>=begin&&at-begin<=size-n&&readable(p,n);
}
// The exact native registration identifies the small accessor that loads the
// current Slate application. Its downstream focus implementation is never called
// by this mod and is deliberately not fingerprinted.
inline bool registeredFocus(const unsigned char* fn,unsigned char* base,size_t size){
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    auto sections=IMAGE_FIRST_SECTION(nt);constexpr char name[]="SetFocusToGameViewport";
    for(unsigned s=0;s<nt->FileHeader.NumberOfSections;++s){
        auto& section=sections[s];if(section.Characteristics&(IMAGE_SCN_MEM_EXECUTE|IMAGE_SCN_MEM_WRITE))continue;
        size_t begin=section.VirtualAddress,n=section.Misc.VirtualSize;
        if(begin>=size||n>size-begin||n<16)continue;
        // Readability is checked per region, not for the whole PE section:
        // loader/hook protection changes may split an otherwise readable section.
        for(size_t i=0;i+16<=n;){
            MEMORY_BASIC_INFORMATION m{};auto at=base+begin+i;
            if(!VirtualQuery(at,&m,sizeof(m))||!m.RegionSize)break;
            auto offset=static_cast<size_t>(at-static_cast<unsigned char*>(m.BaseAddress));
            if(offset>=m.RegionSize)break;
            auto end=i+std::min<size_t>(n-i,m.RegionSize-offset);
            if(m.State==MEM_COMMIT&&!(m.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&(m.Protect&0xee)){
                for(;i<end&&i+16<=n;i+=8){
                    auto pair=base+begin+i;
                    if(i+16>end&&!readable(pair,16))continue;
                    uintptr_t text{},target{};std::memcpy(&text,pair,8);std::memcpy(&target,pair+8,8);
                    if(target==reinterpret_cast<uintptr_t>(fn)&&imageRange(reinterpret_cast<void*>(text),sizeof(name),base,size)&&
                       std::memcmp(reinterpret_cast<void*>(text),name,sizeof(name))==0)return true;
                }
            }else i=(end+7)&~size_t(7);
        }
    }
    return false;
}
inline size_t functionLength(const unsigned char* p,unsigned char* base,size_t size){
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    auto d=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    if(d.VirtualAddress>=size||d.Size>size-d.VirtualAddress||d.Size%sizeof(RUNTIME_FUNCTION)||!readable(base+d.VirtualAddress,d.Size))return 0;
    auto entries=reinterpret_cast<RUNTIME_FUNCTION*>(base+d.VirtualAddress);
    auto at=reinterpret_cast<uintptr_t>(p),begin=reinterpret_cast<uintptr_t>(base);
    if(at<begin||at-begin>=size)return 0;
    auto rva=static_cast<DWORD>(at-begin);size_t low=0,high=d.Size/sizeof(RUNTIME_FUNCTION);
    while(low<high){auto mid=low+(high-low)/2;if(entries[mid].BeginAddress<rva)low=mid+1;else high=mid;}
    if(low==d.Size/sizeof(RUNTIME_FUNCTION)||entries[low].BeginAddress!=rva||entries[low].EndAddress<=rva||entries[low].EndAddress>size)return 0;
    return entries[low].EndAddress-rva;
}
template<size_t N,size_t V>
inline bool entryAbi(const unsigned char* p,const std::array<unsigned char,N>& code,const std::array<unsigned char,N>& mask,
                     const std::array<unsigned char,V>& value,const std::array<unsigned char,V>& valueMask,unsigned char* base,size_t size,std::wstring* error=nullptr){
    auto fail=[&](const std::wstring& why){if(error)*error=why;return false;};
    auto length=functionLength(p,base,size);
    if(length<N)return fail(L"required function entry/boundary unavailable");
    if(!executable(p,N,base,size))return fail(L"required entry code is not readable executable memory");
    for(size_t i=0;i<N;++i)if(mask[i]&&p[i]!=code[i])return fail(L"key/device/handler ABI differs at entry byte "+std::to_wstring(i));
    // Prefix checks the eight-byte key, device argument and handler-to-app
    // adjustment. Also require the fifth argument's typed load and forwarding
    // sequence inside the same function; a matching prologue alone is insufficient.
    // This does not prescribe where event construction or cleanup must live.
    auto end=std::min<size_t>(length,4096);
    for(size_t i=N;i+V<=end;++i)if(executable(p+i,V,base,size)&&p[i]==value[0]&&matches(p+i,value,valueMask))return true;
    return fail(L"typed fifth-argument load/forwarding sequence unavailable");
}
inline std::wstring entryFailure(const wchar_t* name,void* p,unsigned char* image,const std::wstring& reason){
    auto at=reinterpret_cast<uintptr_t>(p),base=reinterpret_cast<uintptr_t>(image);
    return std::wstring(name)+L" (image offset "+(at>=base?std::to_wstring(at-base):L"outside image")+L"): "+reason;
}
inline Binding resolve(std::wstring& error){
    Binding b;
    auto focus=static_cast<unsigned char*>(NativeContract::resolveCode(focusCode,focusMask,[](auto* p,auto* base,size_t n){
        return registeredFocus(p,base,n)&&executable(branch(p+20,1,5),1,base,n);
    },error));
    if(!focus){error=L"Slate application accessor/registration unavailable: "+error;return {};}
    auto image=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(image+reinterpret_cast<IMAGE_DOS_HEADER*>(image)->e_lfanew);auto size=nt->OptionalHeader.SizeOfImage;
    b.application=reinterpret_cast<void**>(const_cast<unsigned char*>(branch(focus+13,3,7)));
    if(reinterpret_cast<uintptr_t>(b.application)%alignof(void*)||!imageRange(b.application,sizeof(void*),image,size)){error=L"Slate application global unavailable or outside image";return {};}
    auto app=*b.application;auto handler=app?static_cast<unsigned char*>(app)+0x148:nullptr;
    auto table=readable(handler,sizeof(void*))?*reinterpret_cast<void***>(handler):nullptr;
    if(!imageRange(table,0xc0,image,size)){error=L"Slate controller message-handler table unavailable or outside image";return {};}
    // Device-aware interface slots, reached through the validated current app.
    b.targets={table[0x98/8],table[0xa8/8],table[0xb8/8]};
    if(b.targets[0]==b.targets[1]||b.targets[0]==b.targets[2]||b.targets[1]==b.targets[2]){error=L"Slate controller entries are not distinct";return {};}
    if(!entryAbi(static_cast<unsigned char*>(b.targets[0]),analogEntryCode,analogEntryMask,analogValueCode,analogValueMask,image,size,&error)){error=entryFailure(L"Slate analog entry",b.targets[0],image,error);return {};}
    for(unsigned i=1;i<3;++i)if(!entryAbi(static_cast<unsigned char*>(b.targets[i]),buttonEntryCode,buttonEntryMask,buttonValueCode,buttonValueMask,image,size,&error)){
        error=entryFailure(i==1?L"Slate button-press entry":L"Slate button-release entry",b.targets[i],image,error);return {};
    }
    return b;
}
inline bool current(const Binding& b,void* handler){return b.application&&readable(b.application,sizeof(void*))&&*b.application&&static_cast<unsigned char*>(*b.application)+0x148==handler;}
}

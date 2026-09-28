#include "rg/steam_hid_hotplug.hpp"
#include "rg/native_input_owner.hpp"
#include <windows.h>
#include <cstring>
namespace rg {
namespace {
// Inspected Steam overlay build. Unknown versions are left untouched. The
// constructor copies sharedConfig->hidMask once; HID/RawInput hooks continue
// using that copy after Steam changes the shared config on a hotplug/setting.
constexpr DWORD overlayTimestamp=0x6ab95eb1, overlaySize=0x1ce000;
constexpr size_t objectRva=0x183cc0, cacheOffset=0x1059c, sharedOffset=0x2e8;
bool accessible(const void* p,size_t size,bool write=false){
    if(!p)return false;
    MEMORY_BASIC_INFORMATION m{};
    if(!VirtualQuery(p,&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    const auto start=reinterpret_cast<std::uintptr_t>(p),base=reinterpret_cast<std::uintptr_t>(m.BaseAddress);
    if(start<base||start-base>m.RegionSize||size>m.RegionSize-(start-base))return false;
    const auto protection=m.Protect&0xff;
    return write?(protection==PAGE_READWRITE||protection==PAGE_WRITECOPY):
        (protection==PAGE_READONLY||protection==PAGE_READWRITE||protection==PAGE_WRITECOPY||
         protection==PAGE_EXECUTE_READ||protection==PAGE_EXECUTE_READWRITE||protection==PAGE_EXECUTE_WRITECOPY);
}
template<size_t N>bool matches(const unsigned char* base,size_t rva,const unsigned char(&code)[N]){
    return rva<=overlaySize&&N<=overlaySize-rva&&accessible(base+rva,N)&&!std::memcmp(base+rva,code,N);
}
bool supported(const unsigned char* base){
    if(!accessible(base,sizeof(IMAGE_DOS_HEADER)))return false;
    auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||dos->e_lfanew>0x1000)return false;
    auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if(!accessible(nt,sizeof(*nt))||nt->Signature!=IMAGE_NT_SIGNATURE||
       nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||nt->FileHeader.TimeDateStamp!=overlayTimestamp||
       nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||nt->OptionalHeader.SizeOfImage!=overlaySize)return false;
    constexpr unsigned char object[]={0x48,0x8d,0x0d,0x8e,0x00,0x0c,0x00};
    constexpr unsigned char mask[]={0x83,0xb9,0x9c,0x05,0x01,0x00,0x00};
    constexpr unsigned char copy[]={0x48,0x8b,0x87,0xe8,0x02,0x00,0x00,0x48,0x85,0xc0,0x74,0x05,0x8b,0x40,0x08,0xeb,0x02,0x8b,0xc6,0x89,0x87,0x9c,0x05,0x01,0x00};
    return matches(base,0xc3c2b,object)&&matches(base,0xabe3f,mask)&&matches(base,0xaac73,copy);
}
// A shared mapping could disappear between VirtualQuery and the read. Keep
// that failure local; never restart/shut down Steam or remove any Steam hook.
bool snapshot(unsigned char* base,std::uintptr_t& shared,std::uint32_t& cached,std::uint32_t& current){
    __try{
        auto object=base+objectRva;
        if(!accessible(object+sharedOffset,sizeof(shared))||!accessible(object+cacheOffset,4,true))return false;
        std::memcpy(&shared,object+sharedOffset,sizeof(shared));
        auto config=reinterpret_cast<unsigned char*>(shared);
        if(shared<0x10000||shared>UINTPTR_MAX-12||!accessible(config+8,4))return false;
        std::memcpy(&cached,object+cacheOffset,4);std::memcpy(&current,config+8,4);
        return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool refresh(unsigned char* base,std::uintptr_t shared,std::uint32_t before,std::uint32_t observedConfig,std::uint32_t after){
    __try{
        std::uintptr_t checkShared{};std::uint32_t checkCached{},checkCurrent{};
        if(!snapshot(base,checkShared,checkCached,checkCurrent)||checkShared!=shared||checkCurrent!=observedConfig||checkCached!=before)return false;
        // Do not clobber a simultaneous update from Steam/another participant.
        auto target=reinterpret_cast<volatile LONG*>(base+objectRva+cacheOffset);
        return static_cast<std::uint32_t>(InterlockedCompareExchange(target,static_cast<LONG>(after),static_cast<LONG>(before)))==before;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
SteamHidHotplug::~SteamHidHotplug(){if(module_)FreeLibrary(static_cast<HMODULE>(module_));}
SteamHidHotplugResult SteamHidHotplug::update(std::uint64_t now){
    if(rejected_||now<nextCheck_)return {};
    nextCheck_=now+250'000'000;
    if(!module_){
        HMODULE module{};
        if(!GetModuleHandleExW(0,L"gameoverlayrenderer64.dll",&module))return {};
        if(!supported(reinterpret_cast<unsigned char*>(module))){FreeLibrary(module);rejected_=true;return {false,false,true};}
        module_=module; // Reference keeps the checked code/data mapped.
    }
    auto base=static_cast<unsigned char*>(module_);
    std::uintptr_t shared{};std::uint32_t cached{},current{};
    if(!snapshot(base,shared,cached,current))return {};
    auto owned=steamVirtualHidMask();if(!owned)return {};
    // Mute an already-open native reader as soon as Steam has a verified output.
    // The Windows visibility filter alone cannot revoke existing HID handles.
    if(*owned)setSteamInputOwner(true);
    auto decision=policy_.observe(cached,current,now,*owned);
    if(decision.refresh){
        // Slot ownership can change independently of shared global preferences.
        if(steamVirtualHidMask()!=owned||!refresh(base,shared,cached,current,decision.value))return {};
        policy_.committed(decision.value);
    }
    if(!*owned&&decision.ready)setSteamInputOwner(false);
    return {decision.rediscover,decision.refresh,false,cached,decision.value};
}
#ifdef RG_HOTPLUG_TEST
bool steamHidSupportedForTest(unsigned char* base){return supported(base);}
bool steamHidSnapshotForTest(unsigned char* base,std::uintptr_t& shared,std::uint32_t& cached,std::uint32_t& current){return snapshot(base,shared,cached,current);}
bool steamHidRefreshForTest(unsigned char* base,std::uintptr_t shared,std::uint32_t before,std::uint32_t after){return refresh(base,shared,before,after,after);}
#endif
}

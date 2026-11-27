#include <Windows.h>
// Only this standalone test translation unit substitutes the host image.
// The product DLL is compiled separately with the real Windows API.
static HMODULE s_testImage = nullptr;
static HMODULE TestGetModuleHandleW(LPCWSTR) { return s_testImage; }
#define GetModuleHandleW TestGetModuleHandleW
#include "../src/CrashGuard.cpp"
#undef GetModuleHandleW
#include <iostream>
#include <vector>
#include <stdexcept>

static unsigned checks = 0;
static void Check(bool value, const char* name) {
    if (!value) throw std::runtime_error(name);
    ++checks; std::cout << "PASS " << name << std::endl;
}
struct Code {
    std::uint8_t* p;
    explicit Code(const std::vector<std::uint8_t>& bytes) {
        p = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE));
        if (!p) throw std::runtime_error("VirtualAlloc");
        std::memcpy(p, bytes.data(), bytes.size());
        DWORD old{}; if(!VirtualProtect(p,4096,PAGE_EXECUTE_READ,&old)) throw std::runtime_error("VirtualProtect");
        FlushInstructionCache(GetCurrentProcess(),p,bytes.size());
    }
    ~Code(){ VirtualFree(p,0,MEM_RELEASE); }
};
static unsigned calls = 0;
static void Target(void*, void* descriptor, void* output) {
    if (!descriptor || !output) std::abort();
    ++calls;
}

static void TestA() {
    // The real null-reference instruction, zeroed local pair, and safe continuation.
    std::vector<std::uint8_t> bytes{
        0x53,0x48,0x83,0xEC,0x30,0x4C,0x8B,0xC1,
        0x0F,0x57,0xC0,0x66,0x0F,0x7F,0x44,0x24,0x20,
        0x49,0x8B,0x40,0x10,0x48,0x85,0xC0,
        0x48,0x89,0x44,0x24,0x28,
        0x48,0x8B,0x44,0x24,0x28,0x48,0x83,0xC4,0x30,0x5B,0xC3
    };
    Code code(bytes); s_guardASafeResume = reinterpret_cast<std::uintptr_t>(code.p+29);
    auto hook=safetyhook::create_mid(code.p+17,OnGuardA);
    Check(static_cast<bool>(hook),"Guard A real SafetyHook installation");
    Check(hook.original_bytes().size()==7,"Guard A seven-byte relocation");
    auto fn=reinterpret_cast<std::uintptr_t(*)(void*)>(code.p);
    auto before=vecg::diag::GuardAHits();
    Check(fn(nullptr)==0,"Guard A NULL resumes with empty local reference");
    Check(vecg::diag::GuardAHits()==before+1,"Guard A NULL counted exactly once");
    std::array<std::uintptr_t,3> object{0,0,0x12345678};
    Check(fn(object.data())==0x12345678,"Guard A normal value preserved");
    Check(vecg::diag::GuardAHits()==before+1,"Guard A normal path not redirected");
    hook.reset();
    Check(std::memcmp(code.p,bytes.data(),bytes.size())==0,"Guard A original instructions restored");
}
static void TestB() {
    // Same argument layout, six-byte movdqa, call [rax+8], and native epilogue.
    std::vector<std::uint8_t> bytes{
        0x53,0x48,0x83,0xEC,0x50,0x85,0xD2,0x75,0x1D,
        0x4C,0x8B,0xC9,0x49,0x8B,0x01,
        0x4C,0x8D,0x44,0x24,0x30,0x48,0x8D,0x54,0x24,0x20,
        0x66,0x0F,0xEF,0xC9,
        0x66,0x0F,0x7F,0x4C,0x24,0x20,
        0xFF,0x50,0x08,0x48,0x83,0xC4,0x50,0x5B,0xC3
    };
    // The branch displacement above must target the native add-rsp at offset 38.
    Check(9+static_cast<std::int8_t>(bytes[8])==38,"Guard B early-return branch target");
    Code code(bytes); s_guardBSafeResume=reinterpret_cast<std::uintptr_t>(code.p+42);
    const std::array<std::uintptr_t,2> table{0,reinterpret_cast<std::uintptr_t>(&Target)};
    const std::array<std::uintptr_t,2> badTarget{0,0};
    const auto lo=std::min(reinterpret_cast<std::uintptr_t>(table.data()),reinterpret_cast<std::uintptr_t>(badTarget.data()));
    const auto hi=std::max(reinterpret_cast<std::uintptr_t>(table.data()),reinterpret_cast<std::uintptr_t>(badTarget.data()))+sizeof(table);
    s_guardBVtableRanges[0]={lo,hi}; s_guardBVtableRangeCount=1;
    s_guardBExecutableRanges[0]={reinterpret_cast<std::uintptr_t>(&Target),reinterpret_cast<std::uintptr_t>(&Target)+1};
    s_guardBExecutableRangeCount=1;
    auto hook=safetyhook::create_mid(code.p+29,OnGuardB,safetyhook::MidHook::StartDisabled);
    Check(static_cast<bool>(hook),"Guard B real SafetyHook installation");
    Check(hook.original_bytes().size()==6,"Guard B relocates only six-byte movdqa");
    Check(std::memcmp(code.p,bytes.data(),bytes.size())==0,"Guard B disabled creation leaves instructions untouched");
    Check(hook.enable().has_value(),"Guard B explicitly enabled after footprint validation");
    Check(std::memcmp(code.p+35,bytes.data()+35,bytes.size()-35)==0,"Guard B call and early-return epilogue remain intact");
    auto fn=reinterpret_cast<void(*)(void*,unsigned)>(code.p);
    auto before=vecg::diag::GuardBHits();
    std::uintptr_t object=reinterpret_cast<std::uintptr_t>(table.data());
    fn(&object,0);
    Check(calls==1 && vecg::diag::GuardBHits()==before,"Guard B normal dispatch completes");
    object=0x00000005FFFFFFFFULL; fn(&object,0);
    Check(calls==1 && vecg::diag::GuardBHits()==before+1,"Guard B known invalid vtable safely skips call");
    object=0x1000; fn(&object,0);
    Check(calls==1 && vecg::diag::GuardBHits()==before+2,"Guard B aligned outside-section vtable safely skips call");
    object=reinterpret_cast<std::uintptr_t>(badTarget.data()); fn(&object,0);
    Check(calls==1 && vecg::diag::GuardBHits()==before+3,"Guard B outside-executable target safely skips call");
    fn(nullptr,1);
    Check(calls==1 && vecg::diag::GuardBHits()==before+3,"Guard B native early return bypasses hook safely");
    for(unsigned i=0;i<10000;++i){object=reinterpret_cast<std::uintptr_t>(table.data());fn(&object,0);object=0x1000;fn(&object,0);fn(nullptr,1);}
    Check(calls==10001 && vecg::diag::GuardBHits()==before+10003,"Guard B 30000 mixed native calls preserve stack and counts");
    hook.reset();
    Check(std::memcmp(code.p,bytes.data(),bytes.size())==0,"Guard B original instructions restored");
}
static unsigned cCalls = 0;
static std::uintptr_t cLastSource = 0;
static void CTarget(void* source) {
    ++cCalls; cLastSource = reinterpret_cast<std::uintptr_t>(source);
}
static void TestC() {
    // Same wrapper, five-byte relative call, return value, saved RBX and stack
    // epilogue as KimuraDev's disassembly; only the callee is a local test stub.
    std::vector<std::uint8_t> bytes{
        0x53,0x48,0x83,0xEC,0x20,
        0x48,0x8B,0xD9,0x48,0x89,0x11,0x48,0x8B,0xCA,
        0xE8,0x2D,0x00,0x00,0x00,
        0x48,0x8B,0xC3,0x48,0x83,0xC4,0x20,0x5B,0xC3
    };
    bytes.resize(64,0x90);
    bytes.insert(bytes.end(),{0x48,0xB8});
    auto target=reinterpret_cast<std::uintptr_t>(&CTarget);
    const auto* targetBytes=reinterpret_cast<const std::uint8_t*>(&target);
    bytes.insert(bytes.end(),targetBytes,targetBytes+sizeof(target));
    bytes.insert(bytes.end(),{0xFF,0xE0});
    Code code(bytes); s_guardCSafeResume=reinterpret_cast<std::uintptr_t>(code.p+19);
    auto hook=safetyhook::create_mid(code.p+14,OnGuardC,safetyhook::MidHook::StartDisabled);
    Check(static_cast<bool>(hook),"Guard C real SafetyHook installation");
    Check(hook.original_bytes().size()==5,"Guard C relocates only five-byte call");
    Check(std::memcmp(code.p,bytes.data(),bytes.size())==0,"Guard C disabled creation leaves instructions untouched");
    Check(hook.enable().has_value(),"Guard C enabled after footprint validation");
    Check(std::memcmp(code.p+19,bytes.data()+19,9)==0,"Guard C safe resume and complete epilogue remain intact");
    using Fn=std::uintptr_t(*)(std::uintptr_t*,std::uintptr_t);
    const auto fn=reinterpret_cast<Fn>(code.p);
    std::array<std::uintptr_t,3> holder{0x11223344,0,0x55667788};
    auto* dest=&holder[1];
    const auto before=vecg::diag::GuardCHits();
    for(auto source:std::array<std::uintptr_t,4>{0,1,0x164,0xFFFF}) {
        holder[1]=~std::uintptr_t{0};
        Check(fn(dest,source)==reinterpret_cast<std::uintptr_t>(dest) && holder[1]==0,
              "Guard C low source returns destination as empty reference");
    }
    Check(cCalls==0 && vecg::diag::GuardCHits()==before+4,"Guard C low sources skip callee and count once each");
    Check(holder[0]==0x11223344 && holder[2]==0x55667788,"Guard C clears only the destination pointer");
    for(auto source:std::array<std::uintptr_t,2>{0x10000,reinterpret_cast<std::uintptr_t>(&holder)}) {
        Check(fn(dest,source)==reinterpret_cast<std::uintptr_t>(dest) && holder[1]==source && cLastSource==source,
              "Guard C normal source preserves call, reference and return value");
    }
    Check(cCalls==2 && vecg::diag::GuardCHits()==before+4,"Guard C normal path adds no redirect counter");
    for(unsigned i=0;i<10000;++i) { fn(dest,0x164); fn(dest,reinterpret_cast<std::uintptr_t>(&holder)); }
    Check(cCalls==10002 && vecg::diag::GuardCHits()==before+10004,
          "Guard C 20000 mixed native calls preserve stack and counts");
    hook.reset();
    Check(std::memcmp(code.p,bytes.data(),bytes.size())==0,"Guard C original instructions restored");

    safetyhook::Context context{};
    context.rcx=0x10000; context.rbx=0x164; context.rip=0x1234;
    const auto original=context;
    OnGuardC(context);
    Check(std::memcmp(&context,&original,sizeof(context))==0,"Guard C normal callback leaves complete saved context unchanged");
    context.rcx=0x164; context.rbx=0x164;
    const auto stack=context.trampoline_rsp;
    OnGuardC(context);
    Check(context.rip==s_guardCSafeResume && context.trampoline_rsp==stack,
          "Guard C low destination avoids write and does not alter trampoline stack");
}

static void TestIndependentCFailure() {
    const auto size=vecg::target::kSizeOfImage;
    auto* image=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,size,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    if(!image) throw std::runtime_error("synthetic PE allocation");
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    dos->e_magic=IMAGE_DOS_SIGNATURE; dos->e_lfanew=0x80;
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(image+0x80);
    nt->Signature=IMAGE_NT_SIGNATURE;
    nt->FileHeader.TimeDateStamp=vecg::target::kPeTimeDateStamp;
    nt->FileHeader.NumberOfSections=2;
    nt->FileHeader.SizeOfOptionalHeader=sizeof(IMAGE_OPTIONAL_HEADER64);
    nt->OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    nt->OptionalHeader.SizeOfImage=size;
    auto* section=IMAGE_FIRST_SECTION(nt);
    section[0].VirtualAddress=0x1000; section[0].Misc.VirtualSize=0x02FF2000;
    section[0].Characteristics=IMAGE_SCN_MEM_EXECUTE|IMAGE_SCN_MEM_READ;
    section[1].VirtualAddress=0x02FF3000; section[1].Misc.VirtualSize=0x00900000;
    section[1].Characteristics=IMAGE_SCN_MEM_READ;
    std::memcpy(image+vecg::target::guard_a::kSignatureRva,vecg::target::guard_a::kSignature.data(),vecg::target::guard_a::kSignature.size());
    std::memcpy(image+vecg::target::guard_b::kSignatureRva,vecg::target::guard_b::kSignature.data(),vecg::target::guard_b::kSignature.size());
    std::memcpy(image+vecg::target::guard_c::kSignatureRva,vecg::target::guard_c::kSignature.data(),vecg::target::guard_c::kSignature.size());
    image[vecg::target::guard_c::kSignatureRva]^=1; // C alone deliberately mismatches.
    FlushInstructionCache(GetCurrentProcess(),image,size);
    s_testImage=reinterpret_cast<HMODULE>(image);
    Check(vecg::InstallCrashGuards(),"Guard C mismatch does not block plugin startup");
    Check(s_guardAHook && s_guardBHook && !s_guardCHook,"Guard C signature failure retains independently installed A/B");
    vecg::RemoveCrashGuards();
    Check(!s_guardAHook && !s_guardBHook && !s_guardCHook,"Independent failure shutdown removes all remaining hooks");
    image[vecg::target::guard_c::kSignatureRva]^=1;
    Check(vecg::InstallCrashGuards() && s_guardAHook && s_guardBHook && s_guardCHook,"All three production installers accept matching synthetic image");
    Check(s_guardBHook.original_bytes().size()==6 && s_guardCHook.original_bytes().size()==5,"Production installers retain required B/C footprints");
    vecg::RemoveCrashGuards();
    Check(std::memcmp(image+vecg::target::guard_c::kSignatureRva,vecg::target::guard_c::kSignature.data(),vecg::target::guard_c::kSignature.size())==0,"Production Guard C removal restores complete original signature");
    nt->FileHeader.TimeDateStamp^=1;
    Check(vecg::InstallCrashGuards() && !s_guardAHook && !s_guardBHook && !s_guardCHook,"Unsupported build leaves every guard inactive");
    vecg::RemoveCrashGuards();
    s_testImage=nullptr; VirtualFree(image,0,MEM_RELEASE);
}
int main(){
    try{TestA();TestB();TestC();TestIndependentCFailure();std::cout<<"ALL "<<checks<<" CHECKS PASSED"<<std::endl;return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
}

#include "CrashGuard.hpp"

#include "BuildTarget.hpp"
#include "Diagnostics.hpp"

#include <Windows.h>

#include <safetyhook.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

namespace
{
safetyhook::MidHook s_guardAHook{};
safetyhook::MidHook s_guardBHook{};
safetyhook::MidHook s_guardCHook{};

std::uintptr_t s_guardASafeResume = 0;
std::uintptr_t s_guardBSafeResume = 0;
std::uintptr_t s_guardCSafeResume = 0;

struct AddressRange
{
    std::uintptr_t begin{0};
    std::uintptr_t end{0};
};

constexpr std::size_t kMaxGuardBSectionRanges = 8;
std::array<AddressRange, kMaxGuardBSectionRanges> s_guardBVtableRanges{};
std::array<AddressRange, kMaxGuardBSectionRanges> s_guardBExecutableRanges{};
std::size_t s_guardBVtableRangeCount = 0;
std::size_t s_guardBExecutableRangeCount = 0;

std::string Hex(std::uint64_t aValue, int aWidth = 0)
{
    std::ostringstream stream;
    stream << "0x" << std::uppercase << std::hex << std::setfill('0');
    if (aWidth > 0)
    {
        stream << std::setw(aWidth);
    }
    stream << aValue;
    return stream.str();
}

bool IsExecutableRange(const IMAGE_NT_HEADERS64* aNtHeaders,
                       std::uintptr_t aRva, std::size_t aSize) noexcept
{
    const auto* section = IMAGE_FIRST_SECTION(aNtHeaders);
    for (std::uint16_t index = 0; index < aNtHeaders->FileHeader.NumberOfSections; ++index, ++section)
    {
        const std::uintptr_t start = section->VirtualAddress;
        const std::uintptr_t span = std::max<std::uint32_t>(section->Misc.VirtualSize,
                                                            section->SizeOfRawData);
        const std::uintptr_t end = start + span;

        if (aRva > UINTPTR_MAX - aSize)
        {
            return false;
        }

        const std::uintptr_t requestedEnd = aRva + aSize;
        if (aRva >= start && requestedEnd <= end)
        {
            return (section->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0 &&
                   (section->Characteristics & IMAGE_SCN_MEM_READ) != 0;
        }
    }

    return false;
}

bool ValidateBuild(std::uint8_t*& aModuleBase, const IMAGE_NT_HEADERS64*& aNtHeaders) noexcept
{
    const HMODULE module = GetModuleHandleW(nullptr);
    if (module == nullptr)
    {
        vecg::diag::Error("Build guard: GetModuleHandleW(nullptr) failed");
        return false;
    }

    aModuleBase = reinterpret_cast<std::uint8_t*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(aModuleBase);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
    {
        vecg::diag::Error("Build guard: invalid DOS header");
        return false;
    }

    aNtHeaders = reinterpret_cast<const IMAGE_NT_HEADERS64*>(aModuleBase + dos->e_lfanew);
    if (aNtHeaders->Signature != IMAGE_NT_SIGNATURE ||
        aNtHeaders->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
    {
        vecg::diag::Error("Build guard: invalid PE64 header");
        return false;
    }

    const std::uint32_t actualTimestamp = aNtHeaders->FileHeader.TimeDateStamp;
    const std::uint32_t actualImageSize = aNtHeaders->OptionalHeader.SizeOfImage;

    vecg::diag::Info("Detected executable base=" +
                     Hex(reinterpret_cast<std::uintptr_t>(aModuleBase), 16) +
                     ", TimeDateStamp=" + Hex(actualTimestamp, 8) +
                     ", SizeOfImage=" + Hex(actualImageSize, 8));

    if (actualTimestamp != vecg::target::kPeTimeDateStamp ||
        actualImageSize != vecg::target::kSizeOfImage)
    {
        vecg::diag::Warning("Build guard: failed - unsupported executable; expected TimeDateStamp=" +
                            Hex(vecg::target::kPeTimeDateStamp, 8) +
                            ", SizeOfImage=" + Hex(vecg::target::kSizeOfImage, 8));
        return false;
    }

    vecg::diag::Info("Build guard: passed");
    return true;
}

template<std::size_t N>
bool ValidateSignature(const std::uint8_t* aModuleBase,
                       const IMAGE_NT_HEADERS64* aNtHeaders,
                       std::string_view aGuardName,
                       std::uintptr_t aSignatureRva,
                       const std::array<std::uint8_t, N>& aSignature) noexcept
{
    const std::uint32_t imageSize = aNtHeaders->OptionalHeader.SizeOfImage;
    if (aSignatureRva > imageSize || aSignature.size() > imageSize - aSignatureRva)
    {
        vecg::diag::Error(std::string(aGuardName) + " signature: range outside SizeOfImage");
        return false;
    }

    if (!IsExecutableRange(aNtHeaders, aSignatureRva, aSignature.size()))
    {
        vecg::diag::Error(std::string(aGuardName) +
                          " signature: target is not inside a readable executable section");
        return false;
    }

    const auto* signatureAddress = aModuleBase + aSignatureRva;
    if (std::memcmp(signatureAddress, aSignature.data(), aSignature.size()) != 0)
    {
        vecg::diag::Warning(std::string(aGuardName) + " signature: mismatch at RVA " +
                            Hex(aSignatureRva, 8) + " - hook disabled independently");
        return false;
    }

    vecg::diag::Info(std::string(aGuardName) + " signature: matched (" +
                     std::to_string(aSignature.size()) + " bytes at RVA " +
                     Hex(aSignatureRva, 8) + ')');
    return true;
}

bool ContainsRange(const std::array<AddressRange, kMaxGuardBSectionRanges>& aRanges,
                   std::size_t aRangeCount,
                   std::uint64_t aAddress,
                   std::size_t aSize) noexcept
{
    if (aSize == 0 || aAddress > UINT64_MAX - aSize)
    {
        return false;
    }

    const std::uint64_t requestedEnd = aAddress + aSize;
    for (std::size_t index = 0; index < aRangeCount; ++index)
    {
        if (aAddress >= aRanges[index].begin && requestedEnd <= aRanges[index].end)
        {
            return true;
        }
    }

    return false;
}

bool AddRange(std::array<AddressRange, kMaxGuardBSectionRanges>& aRanges,
              std::size_t& aRangeCount,
              std::uintptr_t aBegin,
              std::uintptr_t aEnd) noexcept
{
    if (aBegin >= aEnd || aRangeCount >= aRanges.size())
    {
        return false;
    }

    aRanges[aRangeCount++] = AddressRange{aBegin, aEnd};
    return true;
}

bool InitializeGuardBSectionRanges(const std::uint8_t* aModuleBase,
                                   const IMAGE_NT_HEADERS64* aNtHeaders) noexcept
{
    s_guardBVtableRangeCount = 0;
    s_guardBExecutableRangeCount = 0;

    const std::uintptr_t imageBase = reinterpret_cast<std::uintptr_t>(aModuleBase);
    const std::uintptr_t imageSize = aNtHeaders->OptionalHeader.SizeOfImage;
    if (imageBase > UINTPTR_MAX - imageSize)
    {
        return false;
    }

    const std::uintptr_t imageEnd = imageBase + imageSize;
    const auto* section = IMAGE_FIRST_SECTION(aNtHeaders);
    for (std::uint16_t index = 0;
         index < aNtHeaders->FileHeader.NumberOfSections;
         ++index, ++section)
    {
        const std::uintptr_t sectionRva = section->VirtualAddress;
        const std::uintptr_t sectionSize = std::max<std::uint32_t>(
            section->Misc.VirtualSize,
            section->SizeOfRawData);

        if (sectionSize == 0 || sectionRva > imageSize || sectionSize > imageSize - sectionRva)
        {
            continue;
        }

        const std::uintptr_t sectionBegin = imageBase + sectionRva;
        const std::uintptr_t sectionEnd = sectionBegin + sectionSize;
        if (sectionBegin < imageBase || sectionEnd > imageEnd)
        {
            continue;
        }

        const DWORD characteristics = section->Characteristics;
        const bool readable = (characteristics & IMAGE_SCN_MEM_READ) != 0;
        const bool writable = (characteristics & IMAGE_SCN_MEM_WRITE) != 0;
        const bool executable = (characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
        const bool discardable = (characteristics & IMAGE_SCN_MEM_DISCARDABLE) != 0;

        if (readable && !writable && !executable && !discardable)
        {
            bool containsObservedVtable = false;
            for (const std::uintptr_t vtableRva :
                 vecg::target::guard_b::kVtableSectionAnchorRvas)
            {
                if (sectionSize >= sizeof(std::uintptr_t) * 2 &&
                    vtableRva >= sectionRva &&
                    vtableRva - sectionRva <= sectionSize - sizeof(std::uintptr_t) * 2)
                {
                    containsObservedVtable = true;
                    break;
                }
            }

            if (containsObservedVtable &&
                !AddRange(s_guardBVtableRanges,
                          s_guardBVtableRangeCount,
                          sectionBegin,
                          sectionEnd))
            {
                return false;
            }
        }

        if (readable && executable && !discardable)
        {
            if (!AddRange(s_guardBExecutableRanges,
                          s_guardBExecutableRangeCount,
                          sectionBegin,
                          sectionEnd))
            {
                return false;
            }
        }
    }

    if (s_guardBVtableRangeCount == 0 || s_guardBExecutableRangeCount == 0)
    {
        return false;
    }

    for (const std::uintptr_t vtableRva :
         vecg::target::guard_b::kVtableSectionAnchorRvas)
    {
        if (!ContainsRange(s_guardBVtableRanges,
                           s_guardBVtableRangeCount,
                           imageBase + vtableRva,
                           sizeof(std::uintptr_t) * 2))
        {
            return false;
        }
    }

    return true;
}

struct GuardBValidationResult
{
    bool valid{false};
    vecg::diag::GuardBRejectReason reason{
        vecg::diag::GuardBRejectReason::VtableOutsideReadOnlyImage};
    std::uint64_t target{0};
};

GuardBValidationResult ValidateGuardBCall(const safetyhook::Context& aContext) noexcept
{
    GuardBValidationResult result{};

    if ((aContext.rax & (alignof(std::uintptr_t) - 1)) != 0)
    {
        result.reason = vecg::diag::GuardBRejectReason::VtableMisaligned;
        return result;
    }

    if (aContext.rcx != aContext.r9 ||
        aContext.rdx != aContext.rsp + vecg::target::guard_b::kDescriptorStackOffset ||
        aContext.r8 != aContext.rsp + vecg::target::guard_b::kOutputStackOffset)
    {
        result.reason = vecg::diag::GuardBRejectReason::RegisterInvariantMismatch;
        return result;
    }

    if (!ContainsRange(s_guardBVtableRanges,
                       s_guardBVtableRangeCount,
                       aContext.rax,
                       sizeof(std::uintptr_t) * 2))
    {
        result.reason = vecg::diag::GuardBRejectReason::VtableOutsideReadOnlyImage;
        return result;
    }

    std::memcpy(&result.target,
                reinterpret_cast<const void*>(aContext.rax + sizeof(std::uintptr_t)),
                sizeof(result.target));

    if (!ContainsRange(s_guardBExecutableRanges,
                       s_guardBExecutableRangeCount,
                       result.target,
                       1))
    {
        result.reason = vecg::diag::GuardBRejectReason::TargetOutsideExecutableImage;
        return result;
    }

    result.valid = true;
    return result;
}

void OnGuardA(safetyhook::Context& aContext) noexcept
{
    if (aContext.r8 != 0)
    {
        return;
    }

    vecg::diag::RecordGuardAHit();

    // At this point [rsp+0x20] and [rsp+0x28] were already zeroed by native
    // code. 0x58D5A8 is the game's own continuation for an empty reference.
    aContext.rip = s_guardASafeResume;
}

void OnGuardB(safetyhook::Context& aContext) noexcept
{
    const GuardBValidationResult validation = ValidateGuardBCall(aContext);
    if (validation.valid)
    {
        return;
    }

    vecg::diag::RecordGuardBHit(validation.reason,
                                aContext.rcx,
                                aContext.rax,
                                validation.target);

    // SafetyHook v0.7.0 relocates only the six-byte movdqa preceding
    // call [rax+8], preserving both native early-return branches.
    // Context::rsp is read-only, so retain the proven
    // cleanup through trampoline_rsp: stage the intact pop-rbx address at
    // [original_rsp+0x48]. The mid-hook stub's final ret consumes that address,
    // leaving the real RSP at original_rsp+0x50 before execution reaches pop rbx.
    const std::uintptr_t resumeAddressSlot =
        aContext.rsp + vecg::target::guard_b::kResumeAddressSlotOffset;
    std::memcpy(reinterpret_cast<void*>(resumeAddressSlot),
                &s_guardBSafeResume,
                sizeof(s_guardBSafeResume));

    aContext.trampoline_rsp = resumeAddressSlot;
    aContext.rip = s_guardBSafeResume;
}
// Guard C by Gustavo Kimura (KimuraDev), PR #1. Callback retained verbatim.
void OnGuardC(safetyhook::Context& aContext) noexcept
{
    // Normal case: rcx is a valid heap/stack pointer in 64-bit user space (>= 64 KB).
    if (aContext.rcx >= 0x10000)
    {
        return;
    }

    // Invalid/null-offset context pointer (e.g. 0x164).
    vecg::diag::RecordGuardCHit(aContext.rcx, aContext.rbx);

    // rbx points to the destination reference struct.
    // Zero out the pointer to make it a safe, valid empty reference.
    if (aContext.rbx >= 0x10000)
    {
        *reinterpret_cast<std::uintptr_t*>(aContext.rbx) = 0;
    }

    // Skip the crashing call to 0x22CA420 and jump directly to the epilogue at 0x1FFADB4.
    aContext.rip = s_guardCSafeResume;
}
} // namespace

namespace vecg
{
bool InstallCrashGuards() noexcept
{
    diag::StartupStatus status{};

    try
    {
        std::uint8_t* moduleBase = nullptr;
        const IMAGE_NT_HEADERS64* ntHeaders = nullptr;

        status.buildGuardPassed = ValidateBuild(moduleBase, ntHeaders);
        if (status.buildGuardPassed)
        {
            status.guardASignatureMatched = ValidateSignature(
                moduleBase,
                ntHeaders,
                target::guard_a::kName,
                target::guard_a::kSignatureRva,
                target::guard_a::kSignature);

            if (status.guardASignatureMatched)
            {
                s_guardASafeResume = reinterpret_cast<std::uintptr_t>(moduleBase) +
                                     target::guard_a::kSafeResumeRva;
                s_guardAHook = safetyhook::create_mid(moduleBase + target::guard_a::kHookRva,
                                                      OnGuardA);
                status.guardAHookInstalled = static_cast<bool>(s_guardAHook);

                if (status.guardAHookInstalled)
                {
                    diag::Info("Guard A hook: installed at RVA " +
                               Hex(target::guard_a::kHookRva, 8) +
                               ", safe resume RVA " +
                               Hex(target::guard_a::kSafeResumeRva, 8));
                }
                else
                {
                    s_guardASafeResume = 0;
                    diag::Error("Guard A hook: installation failed");
                }
            }

            status.guardBSignatureMatched = ValidateSignature(
                moduleBase,
                ntHeaders,
                target::guard_b::kName,
                target::guard_b::kSignatureRva,
                target::guard_b::kSignature);

            if (status.guardBSignatureMatched)
            {
                if (!InitializeGuardBSectionRanges(moduleBase, ntHeaders))
                {
                    diag::Error("Guard B static PE ranges: initialization failed; hook disabled");
                }
                else
                {
                    diag::Info("Guard B static PE ranges: read-only=" +
                               std::to_string(s_guardBVtableRangeCount) +
                               ", executable=" +
                               std::to_string(s_guardBExecutableRangeCount));

                    s_guardBSafeResume = reinterpret_cast<std::uintptr_t>(moduleBase) +
                                         target::guard_b::kSafeResumeRva;
                    s_guardBHook = safetyhook::create_mid(moduleBase + target::guard_b::kHookRva,
                                                          OnGuardB,
                                                          safetyhook::MidHook::StartDisabled);
                    // Validate the relocation footprint before patching executable code.
                    // Native branches enter the epilogue immediately after the call.
                    if (s_guardBHook)
                    {
                        if (s_guardBHook.original_bytes().size() != 6)
                        {
                            diag::Error("Guard B hook: unexpected overwrite size; disabled");
                            s_guardBHook.reset();
                        }
                        else if (!s_guardBHook.enable())
                        {
                            s_guardBHook.reset();
                        }
                    }
                    status.guardBHookInstalled = static_cast<bool>(s_guardBHook);

                    if (status.guardBHookInstalled)
                    {
                        diag::Info("Guard B hook: installed at RVA " +
                                   Hex(target::guard_b::kHookRva, 8) +
                                   ", safe resume RVA " +
                                   Hex(target::guard_b::kSafeResumeRva, 8));
                    }
                    else
                    {
                        s_guardBSafeResume = 0;
                        diag::Error("Guard B hook: installation failed");
                    }
                }
            }
            status.guardCSignatureMatched = ValidateSignature(
                moduleBase,
                ntHeaders,
                target::guard_c::kName,
                target::guard_c::kSignatureRva,
                target::guard_c::kSignature);

            if (status.guardCSignatureMatched)
            {
                s_guardCSafeResume = reinterpret_cast<std::uintptr_t>(moduleBase) +
                                     target::guard_c::kSafeResumeRva;
                s_guardCHook = safetyhook::create_mid(moduleBase + target::guard_c::kHookRva,
                                                      OnGuardC,
                                                      safetyhook::MidHook::StartDisabled);
                // The safe resume is immediately after the five-byte call.
                // Refuse any larger relocation before writing executable code.
                if (s_guardCHook)
                {
                    if (s_guardCHook.original_bytes().size() != 5)
                    {
                        diag::Error("Guard C hook: unexpected overwrite size; disabled");
                        s_guardCHook.reset();
                    }
                    else if (!s_guardCHook.enable())
                    {
                        s_guardCHook.reset();
                    }
                }
                status.guardCHookInstalled = static_cast<bool>(s_guardCHook);

                if (status.guardCHookInstalled)
                {
                    diag::Info("Guard C hook: installed at RVA " +
                               Hex(target::guard_c::kHookRva, 8) +
                               ", safe resume RVA " +
                               Hex(target::guard_c::kSafeResumeRva, 8));
                }
                else
                {
                    s_guardCSafeResume = 0;
                    diag::Error("Guard C hook: installation failed");
                }
            }
        }
        else
        {
            diag::Warning("Guards remain inactive in fail-closed mode");
        }
    }
    catch (...)
    {
        if (s_guardCHook)
        {
            s_guardCHook.reset();
        }
        if (s_guardBHook)
        {
            s_guardBHook.reset();
        }
        if (s_guardAHook)
        {
            s_guardAHook.reset();
        }

        s_guardASafeResume = 0;
        s_guardBSafeResume = 0;
        s_guardCSafeResume = 0;
        s_guardBVtableRangeCount = 0;
        s_guardBExecutableRangeCount = 0;
        status.guardAHookInstalled = false;
        status.guardBHookInstalled = false;
        status.guardCHookInstalled = false;
        diag::Error("Unexpected initialization failure; no uncertain hook remains installed");
    }

    if (!diag::StartMonitor(status))
    {
        diag::Warning("Hooks may remain active, but live activation reporting is unavailable");
    }

    diag::Info("Initialization complete: Guard A installed=" +
               std::string(status.guardAHookInstalled ? "yes" : "no") +
               ", Guard B installed=" +
               std::string(status.guardBHookInstalled ? "yes" : "no") +
               ", Guard C installed=" +
               std::string(status.guardCHookInstalled ? "yes" : "no"));

    // Fail-closed errors disable only the affected guard and never block startup.
    return true;
}

void RemoveCrashGuards() noexcept
{
    try
    {
        // Remove hooks before closing the monitor event so no callback can signal
        // an already-destroyed handle.
        if (s_guardCHook)
        {
            s_guardCHook.reset();
        }
        if (s_guardBHook)
        {
            s_guardBHook.reset();
        }
        if (s_guardAHook)
        {
            s_guardAHook.reset();
        }

        diag::Info("Removing guards. Guard A redirects=" +
                   std::to_string(diag::GuardAHits()) +
                   ", Guard B redirects=" +
                   std::to_string(diag::GuardBHits()) +
                   ", Guard C redirects=" +
                   std::to_string(diag::GuardCHits()));
        diag::StopMonitor();
    }
    catch (...)
    {
        diag::Error("Unexpected shutdown failure while removing guards");
    }

    s_guardASafeResume = 0;
    s_guardBSafeResume = 0;
    s_guardCSafeResume = 0;
    s_guardBVtableRangeCount = 0;
    s_guardBExecutableRangeCount = 0;
}
} // namespace vecg

#pragma once

#include <RED4ext/Api/v1/Logger.hpp>
#include <RED4ext/Api/v1/PluginHandle.hpp>

#include <cstdint>
#include <string_view>

namespace vecg::diag
{
struct StartupStatus
{
    bool buildGuardPassed{false};
    bool guardASignatureMatched{false};
    bool guardAHookInstalled{false};
    bool guardBSignatureMatched{false};
    bool guardBHookInstalled{false};
    bool guardCSignatureMatched{false};
    bool guardCHookInstalled{false};
};

void Initialize(RED4ext::v1::PluginHandle aHandle, const RED4ext::v1::Logger* aLogger) noexcept;
void Info(std::string_view aMessage) noexcept;
void Warning(std::string_view aMessage) noexcept;
void Error(std::string_view aMessage) noexcept;

bool StartMonitor(const StartupStatus& aStatus) noexcept;
void StopMonitor() noexcept;

enum class GuardBRejectReason : std::uint32_t
{
    VtableMisaligned = 1,
    RegisterInvariantMismatch,
    VtableOutsideReadOnlyImage,
    TargetOutsideExecutableImage,
};

void RecordGuardAHit() noexcept;
void RecordGuardBHit(GuardBRejectReason aReason,
                     std::uint64_t aObject,
                     std::uint64_t aVtable,
                     std::uint64_t aTarget) noexcept;
void RecordGuardCHit(std::uint64_t aBadPtr,
                     std::uint64_t aDestPtr) noexcept;

std::uint64_t GuardAHits() noexcept;
std::uint64_t GuardBHits() noexcept;
std::uint64_t GuardCHits() noexcept;
} // namespace vecg::diag

#include "BuildTarget.hpp"
#include "CrashGuard.hpp"
#include "Diagnostics.hpp"

#include <RED4ext/RED4ext.hpp>

RED4EXT_C_EXPORT bool RED4EXT_CALL Main(RED4ext::v1::PluginHandle aHandle,
                                        RED4ext::v1::EMainReason aReason,
                                        const RED4ext::v1::Sdk* aSdk)
{
    switch (aReason)
    {
    case RED4ext::v1::EMainReason::Load:
        vecg::diag::Initialize(aHandle, aSdk != nullptr ? aSdk->logger : nullptr);
        vecg::diag::Info("v1.0 Load entered");
        return vecg::InstallCrashGuards();

    case RED4ext::v1::EMainReason::Unload:
        vecg::RemoveCrashGuards();
        break;
    }

    return true;
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* aInfo)
{
    aInfo->name = vecg::target::kPluginName;
    aInfo->author = vecg::target::kAuthor;
    aInfo->version = RED4EXT_V1_SEMVER(1, 0, 0);
    aInfo->runtime = RED4EXT_V1_RUNTIME_VERSION_2_31;
    aInfo->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
}

RED4EXT_C_EXPORT std::uint32_t RED4EXT_CALL Supports()
{
    return RED4EXT_API_VERSION_1;
}

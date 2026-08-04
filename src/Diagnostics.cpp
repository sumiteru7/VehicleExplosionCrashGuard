#include "Diagnostics.hpp"

#include <Windows.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>

namespace
{
RED4ext::v1::PluginHandle s_pluginHandle{};
const RED4ext::v1::Logger* s_logger = nullptr;

std::atomic<std::uint64_t> s_guardAHits{0};
std::atomic<std::uint64_t> s_guardBHits{0};
std::atomic<std::uint32_t> s_lastGuardBReason{0};
std::atomic<std::uint64_t> s_lastGuardBObject{0};
std::atomic<std::uint64_t> s_lastGuardBVtable{0};
std::atomic<std::uint64_t> s_lastGuardBTarget{0};

std::atomic<HANDLE> s_activityEvent{nullptr};
std::jthread s_monitorThread{};

std::string Prefix(std::string_view aMessage)
{
    return "VehicleExplosionCrashGuard: " + std::string(aMessage);
}

std::string Hex(std::uint64_t aValue, int aWidth = 16)
{
    std::ostringstream stream;
    stream << "0x" << std::uppercase << std::hex << std::setfill('0')
           << std::setw(aWidth) << aValue;
    return stream.str();
}

std::string_view GuardBRejectReasonText(vecg::diag::GuardBRejectReason aReason) noexcept
{
    using vecg::diag::GuardBRejectReason;

    switch (aReason)
    {
    case GuardBRejectReason::VtableMisaligned:
        return "vtable-misaligned";
    case GuardBRejectReason::RegisterInvariantMismatch:
        return "register-invariant-mismatch";
    case GuardBRejectReason::VtableOutsideReadOnlyImage:
        return "vtable-outside-read-only-image";
    case GuardBRejectReason::TargetOutsideExecutableImage:
        return "target-outside-executable-image";
    }

    return "unknown";
}

std::string Timestamp()
{
    SYSTEMTIME time{};
    GetLocalTime(&time);

    std::ostringstream stream;
    stream << std::setfill('0')
           << std::setw(4) << time.wYear << '-'
           << std::setw(2) << time.wMonth << '-'
           << std::setw(2) << time.wDay << ' '
           << std::setw(2) << time.wHour << ':'
           << std::setw(2) << time.wMinute << ':'
           << std::setw(2) << time.wSecond << '.'
           << std::setw(3) << time.wMilliseconds;
    return stream.str();
}

std::filesystem::path DedicatedLogPath()
{
    wchar_t executablePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, executablePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        return {};
    }

    // <game>/bin/x64/Cyberpunk2077.exe -> <game>/red4ext/logs
    const auto gameRoot = std::filesystem::path(executablePath)
                              .parent_path()
                              .parent_path()
                              .parent_path();
    return gameRoot / L"red4ext" / L"logs" / L"VehicleExplosionCrashGuard.log";
}

void WriteLine(std::ofstream& aFile, std::string_view aLevel, std::string_view aMessage)
{
    if (!aFile)
    {
        return;
    }

    aFile << '[' << Timestamp() << "] [" << aLevel << "] " << aMessage << '\n';
    aFile.flush();
}

const char* YesNo(bool aValue) noexcept
{
    return aValue ? "yes" : "no";
}

void MonitorMain(std::stop_token aStopToken, vecg::diag::StartupStatus aStatus) noexcept
{
    try
    {
        const HANDLE activityEvent = s_activityEvent.load(std::memory_order_acquire);
        if (activityEvent == nullptr)
        {
            vecg::diag::Error("Diagnostic monitor started without an activity event.");
            return;
        }

        std::ofstream file;
        const auto logPath = DedicatedLogPath();
        if (!logPath.empty())
        {
            std::filesystem::create_directories(logPath.parent_path());
            file.open(logPath, std::ios::out | std::ios::trunc);
        }

        if (file)
        {
            WriteLine(file, "INFO", "VehicleExplosionCrashGuard v1.0 diagnostic session started.");
            WriteLine(file, "INFO", std::string("Build guard passed=") + YesNo(aStatus.buildGuardPassed) + '.');
            WriteLine(file, "INFO", std::string("Guard A signature matched=") + YesNo(aStatus.guardASignatureMatched) + '.');
            WriteLine(file, "INFO", std::string("Guard A hook installed=") + YesNo(aStatus.guardAHookInstalled) + '.');
            WriteLine(file, "INFO", std::string("Guard B signature matched=") + YesNo(aStatus.guardBSignatureMatched) + '.');
            WriteLine(file, "INFO", std::string("Guard B hook installed=") + YesNo(aStatus.guardBHookInstalled) + '.');
        }
        else
        {
            vecg::diag::Warning("Dedicated diagnostic log could not be opened; RED4ext main log remains authoritative.");
        }

        vecg::diag::Info("Diagnostic monitor: started");

        std::uint64_t reportedA = 0;
        std::uint64_t reportedB = 0;

        const auto reportChanges = [&]()
        {
            const auto currentA = s_guardAHits.load(std::memory_order_acquire);
            const auto currentB = s_guardBHits.load(std::memory_order_acquire);

            if (currentA != reportedA)
            {
                const std::string message = "Guard A redirect detected. Total=" + std::to_string(currentA) + '.';
                vecg::diag::Info(message);
                WriteLine(file, "INFO", message);
                reportedA = currentA;
            }

            if (currentB != reportedB)
            {
                const auto reason = static_cast<vecg::diag::GuardBRejectReason>(
                    s_lastGuardBReason.load(std::memory_order_acquire));
                const auto object = s_lastGuardBObject.load(std::memory_order_acquire);
                const auto vtable = s_lastGuardBVtable.load(std::memory_order_acquire);
                const auto target = s_lastGuardBTarget.load(std::memory_order_acquire);

                const std::string message =
                    "Guard B redirect detected. Total=" + std::to_string(currentB) +
                    ", reason=" + std::string(GuardBRejectReasonText(reason)) +
                    ", object=" + Hex(object) +
                    ", vtable=" + Hex(vtable) +
                    ", target=" + Hex(target) + '.';
                vecg::diag::Info(message);
                WriteLine(file, "INFO", message);
                reportedB = currentB;
            }
        };

        // Catch a hit that occurred in the narrow window between hook
        // installation and monitor-thread startup.
        reportChanges();

        while (!aStopToken.stop_requested())
        {
            const DWORD waitResult = WaitForSingleObject(activityEvent, 1000);
            if (aStopToken.stop_requested())
            {
                break;
            }

            if (waitResult == WAIT_TIMEOUT)
            {
                continue;
            }

            if (waitResult != WAIT_OBJECT_0)
            {
                vecg::diag::Warning("Diagnostic monitor wait failed; live hit reporting stopped.");
                WriteLine(file, "WARN", "Diagnostic monitor wait failed; live hit reporting stopped.");
                break;
            }

            reportChanges();
        }

        const auto finalA = s_guardAHits.load(std::memory_order_acquire);
        const auto finalB = s_guardBHits.load(std::memory_order_acquire);
        WriteLine(file, "INFO", "Diagnostic monitor stopped. Guard A redirects=" +
                                     std::to_string(finalA) + ", Guard B redirects=" +
                                     std::to_string(finalB) + '.');
    }
    catch (...)
    {
        vecg::diag::Error("Unexpected diagnostic monitor failure.");
    }
}

void SignalMonitor() noexcept
{
    const HANDLE eventHandle = s_activityEvent.load(std::memory_order_acquire);
    if (eventHandle != nullptr)
    {
        SetEvent(eventHandle);
    }
}
} // namespace

namespace vecg::diag
{
void Initialize(RED4ext::v1::PluginHandle aHandle, const RED4ext::v1::Logger* aLogger) noexcept
{
    s_pluginHandle = aHandle;
    s_logger = aLogger;
    s_guardAHits.store(0, std::memory_order_relaxed);
    s_guardBHits.store(0, std::memory_order_relaxed);
    s_lastGuardBReason.store(0, std::memory_order_relaxed);
    s_lastGuardBObject.store(0, std::memory_order_relaxed);
    s_lastGuardBVtable.store(0, std::memory_order_relaxed);
    s_lastGuardBTarget.store(0, std::memory_order_relaxed);
}

void Info(std::string_view aMessage) noexcept
{
    try
    {
        if (s_logger != nullptr && s_logger->Info != nullptr)
        {
            const std::string message = Prefix(aMessage);
            s_logger->Info(s_pluginHandle, message.c_str());
        }
    }
    catch (...)
    {
    }
}

void Warning(std::string_view aMessage) noexcept
{
    try
    {
        if (s_logger != nullptr && s_logger->Warn != nullptr)
        {
            const std::string message = Prefix(aMessage);
            s_logger->Warn(s_pluginHandle, message.c_str());
        }
    }
    catch (...)
    {
    }
}

void Error(std::string_view aMessage) noexcept
{
    try
    {
        if (s_logger != nullptr && s_logger->Error != nullptr)
        {
            const std::string message = Prefix(aMessage);
            s_logger->Error(s_pluginHandle, message.c_str());
        }
    }
    catch (...)
    {
    }
}

bool StartMonitor(const StartupStatus& aStatus) noexcept
{
    try
    {
        if (s_monitorThread.joinable())
        {
            Warning("Diagnostic monitor was already running.");
            return true;
        }

        const HANDLE activityEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (activityEvent == nullptr)
        {
            Error("Diagnostic monitor: failed to create activity event.");
            return false;
        }

        s_activityEvent.store(activityEvent, std::memory_order_release);
        s_monitorThread = std::jthread(MonitorMain, aStatus);
        return true;
    }
    catch (...)
    {
        const HANDLE activityEvent = s_activityEvent.exchange(nullptr, std::memory_order_acq_rel);
        if (activityEvent != nullptr)
        {
            CloseHandle(activityEvent);
        }
        Error("Diagnostic monitor: failed to start.");
        return false;
    }
}

void StopMonitor() noexcept
{
    try
    {
        if (s_monitorThread.joinable())
        {
            s_monitorThread.request_stop();
            SignalMonitor();
            s_monitorThread.join();
        }

        const HANDLE activityEvent = s_activityEvent.exchange(nullptr, std::memory_order_acq_rel);
        if (activityEvent != nullptr)
        {
            CloseHandle(activityEvent);
        }
    }
    catch (...)
    {
        Error("Unexpected failure while stopping diagnostic monitor.");
    }
}

void RecordGuardAHit() noexcept
{
    s_guardAHits.fetch_add(1, std::memory_order_release);
    SignalMonitor();
}

void RecordGuardBHit(GuardBRejectReason aReason,
                     std::uint64_t aObject,
                     std::uint64_t aVtable,
                     std::uint64_t aTarget) noexcept
{
    s_lastGuardBReason.store(static_cast<std::uint32_t>(aReason), std::memory_order_relaxed);
    s_lastGuardBObject.store(aObject, std::memory_order_relaxed);
    s_lastGuardBVtable.store(aVtable, std::memory_order_relaxed);
    s_lastGuardBTarget.store(aTarget, std::memory_order_relaxed);
    s_guardBHits.fetch_add(1, std::memory_order_release);
    SignalMonitor();
}

std::uint64_t GuardAHits() noexcept
{
    return s_guardAHits.load(std::memory_order_acquire);
}

std::uint64_t GuardBHits() noexcept
{
    return s_guardBHits.load(std::memory_order_acquire);
}
} // namespace vecg::diag

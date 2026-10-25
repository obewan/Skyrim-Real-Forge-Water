#include "Plugin.h"
#include "Hooks.h"
#include "Menu.h"
#include <spdlog/sinks/basic_file_sink.h>

void SetupLog() {
    auto logsFolder = SKSE::log::log_directory();
    if (!logsFolder) {
        SKSE::stl::report_and_fail("SKSE log_directory not provided, logs disabled.");
        return;
    }

    auto logFilePath = *logsFolder / std::format("{}.log", Plugin::NAME);
    auto fileLoggerPtr = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);
    auto loggerPtr = std::make_shared<spdlog::logger>("log", std::move(fileLoggerPtr));
    spdlog::set_default_logger(std::move(loggerPtr));
    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_on(spdlog::level::info);
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    // Init first: it resolves the runtime module that log_directory() needs.
    // Pass false so CommonLibSSE does not install its own logger over ours.
    SKSE::Init(skse, false);

    SetupLog();

    SKSE::log::info(("{} v{}"), Plugin::NAME, Plugin::VERSION);
    SKSE::log::info("Game version : {}", skse->RuntimeVersion().string());

    Settings::Load();

    // The vtable hooks do not allocate trampoline space, so it is safe to install them early.
    Hooks::Install();

    // Event sources only exist once the game data is loaded.
    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message *message) {
        switch (message->type) {
            case SKSE::MessagingInterface::kDataLoaded:
                ForgeScanner::RegisterCellHandlers();
                WaterConverter::RefreshWaterType();
                Menu::Register();
                break;
            // Read the INI again before each load, so the water look can be tuned without restarting.
            case SKSE::MessagingInterface::kPreLoadGame:
            case SKSE::MessagingInterface::kNewGame:
                Settings::Load();
                WaterConverter::RefreshWaterType();
                break;
            default:
                break;
        }
    });

    return true;
}

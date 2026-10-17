#pragma once

#include <unordered_map>

#include <SimpleIni.h>

#include "Plugin.h"
#include "WaterLook.h"

// Data/SKSE/Plugins/RealForgeWater.ini. Every value has a default, so the file is optional.
// The file is read again before each save load, so the water look can be tuned without restarting.
namespace Settings {

    inline const std::string FILE = std::format("Data/SKSE/Plugins/{}.ini", Plugin::NAME);

    // 0 = detect and log only, 1 = turn forge water into real water.
    inline int mode = 1;

    // Water type (WATR) the forge water type is copied from. Default: the vanilla horse trough water.
    inline RE::FormID waterTypeID = 0x000F762D;
    inline std::string waterTypePlugin = "Skyrim.esm";

    // Wave texture of the forge water's 3 noise layers, like "Data\Textures\Water\DefaultWater.dds".
    // Empty = the water type's own, which for the vanilla trough water is the game's global wave
    // texture (replaced by water mods such as Realistic Water Two, for all water).
    inline std::string waveTexture;

    // The vanilla wave texture, shipped with the plugin under its own path so that water mods, which
    // replace the global one, do not replace it.
    inline constexpr auto VANILLA_WAVE_TEXTURE = "Data\\Textures\\RealForgeWater\\Water\\DefaultWater.dds";

    // Log every shape of every forge mesh and more details, for bug reports.
    inline bool verboseLog = false;

    // Unregister and hide other real water found in a converted trough (Realistic Water Two adds some
    // at every vanilla forge, right on top of ours).
    inline bool hideOtherTroughWater = true;

    // [WaterLook] overrides applied on the copied water type, by INI key (see WaterLook.h).
    // A missing or empty key keeps the value of the source water type.
    inline std::unordered_map<std::string, float> floatOverrides;
    inline std::unordered_map<std::string, RE::Color> colorOverrides;

    // "R, G, B" with 0-255 components.
    inline std::optional<RE::Color> ParseColor(const char* a_key, const char* a_value) {
        int r = 0, g = 0, b = 0;
        if (std::sscanf(a_value, "%d , %d , %d", &r, &g, &b) != 3) {
            SKSE::log::warn("[WaterLook] {} = \"{}\" is not \"R, G, B\", ignored", a_key, a_value);
            return std::nullopt;
        }
        return RE::Color(static_cast<std::uint8_t>(std::clamp(r, 0, 255)), static_cast<std::uint8_t>(std::clamp(g, 0, 255)),
                         static_cast<std::uint8_t>(std::clamp(b, 0, 255)), 255);
    }

    inline void Load() {
        CSimpleIniA ini;
        ini.SetUnicode();
        if (ini.LoadFile(FILE.c_str()) < 0) {
            SKSE::log::info("{} not found, using defaults", FILE);
        }

        mode = static_cast<int>(ini.GetLongValue("General", "iMode", 1));
        verboseLog = ini.GetBoolValue("General", "bVerboseLog", false);
        hideOtherTroughWater = ini.GetBoolValue("General", "bHideOtherTroughWater", true);
        waterTypeID = static_cast<RE::FormID>(
            std::strtoul(ini.GetValue("Water", "sWaterTypeFormID", "0x000F762D"), nullptr, 16));
        waterTypePlugin = ini.GetValue("Water", "sWaterTypePlugin", "Skyrim.esm");
        waveTexture = ini.GetValue("Water", "sWaveTexture", VANILLA_WAVE_TEXTURE);

        floatOverrides.clear();
        for (const auto& field : WaterLook::FLOATS) {
            const char* value = ini.GetValue("WaterLook", field.key, nullptr);
            if (value && *value) floatOverrides[field.key] = std::strtof(value, nullptr);
        }
        colorOverrides.clear();
        for (const auto& field : WaterLook::COLORS) {
            const char* value = ini.GetValue("WaterLook", field.key, nullptr);
            if (value && *value) {
                if (auto color = ParseColor(field.key, value)) colorOverrides[field.key] = *color;
            }
        }

        SKSE::log::info("Settings: iMode={} bHideOtherTroughWater={} bVerboseLog={} water type {:06X} in {}, "
                        "{} look overrides, wave texture \"{}\"",
                        mode, hideOtherTroughWater, verboseLog, waterTypeID & 0xFFFFFF, waterTypePlugin,
                        floatOverrides.size() + colorOverrides.size(), waveTexture);
    }

    // Writes every [WaterLook] value of a_data to the INI, keeping the file's other lines and comments.
    inline bool SaveLook(const RE::WaterShaderData& a_data) {
        CSimpleIniA ini;
        ini.SetUnicode();
        ini.LoadFile(FILE.c_str());

        auto& data = const_cast<RE::WaterShaderData&>(a_data);
        for (const auto& field : WaterLook::FLOATS) {
            const float value = field.get(data);
            ini.SetValue("WaterLook", field.key, std::format("{}", value).c_str());
            floatOverrides[field.key] = value;
        }
        for (const auto& field : WaterLook::COLORS) {
            const auto& color = field.get(data);
            ini.SetValue("WaterLook", field.key, std::format("{}, {}, {}", color.red, color.green, color.blue).c_str());
            colorOverrides[field.key] = color;
        }
        ini.SetBoolValue("General", "bHideOtherTroughWater", hideOtherTroughWater);
        ini.SetValue("Water", "sWaveTexture", waveTexture.c_str());
        return ini.SaveFile(FILE.c_str()) >= 0;
    }
}

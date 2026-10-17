#pragma once

// In-game menu, with SKSE Menu Framework (optional): every water look setting as a live slider,
// a switch to compare with the water other mods put in forge troughs (Realistic Water Two), and
// save / reload buttons for the INI.
//
// The menu draws on the render thread: it only edits plain values of the forge water type and
// leaves the game calls to the main thread (WaterConverter::RequestRefresh and friends).

#include "SKSEMenuFramework.h"
#undef GetObject
#undef GetClassName
#undef GetMessage

#include "WaterConverter.h"

namespace Menu {

    inline bool FloatSlider(const WaterLook::FloatField& a_field, RE::WaterShaderData& a_data) {
        const bool changed = ImGui::SliderFloat(a_field.label, &a_field.get(a_data), a_field.min, a_field.max, a_field.format);
        if (a_field.tooltip) ImGui::SetItemTooltip("%s", a_field.tooltip);
        return changed;
    }

    inline bool ColorPicker(const WaterLook::ColorField& a_field, RE::WaterShaderData& a_data) {
        auto& color = a_field.get(a_data);
        float rgb[3]{color.red / 255.0f, color.green / 255.0f, color.blue / 255.0f};
        if (!ImGui::ColorEdit3(a_field.label, rgb)) return false;
        color.red = static_cast<std::uint8_t>(std::lround(rgb[0] * 255.0f));
        color.green = static_cast<std::uint8_t>(std::lround(rgb[1] * 255.0f));
        color.blue = static_cast<std::uint8_t>(std::lround(rgb[2] * 255.0f));
        return true;
    }

    inline bool Group(const char* a_title, WaterLook::Group a_group, RE::WaterShaderData& a_data, bool a_open = false) {
        bool changed = false;
        if (ImGui::CollapsingHeader(a_title, a_open ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
            ImGui::PushID(a_title);
            for (const auto& field : WaterLook::COLORS) {
                if (field.group == a_group) changed |= ColorPicker(field, a_data);
            }
            for (const auto& field : WaterLook::FLOATS) {
                if (field.group == a_group) changed |= FloatSlider(field, a_data);
            }
            ImGui::PopID();
        }
        return changed;
    }

    inline void __stdcall RenderWater() {
        auto water = WaterConverter::forgeWater;
        if (!water) {
            ImGui::TextWrapped("The forge water type is not ready yet: load a save first.");
            return;
        }

        ImGui::TextWrapped("Look of the real water in forge troughs. Changes show when the menu is closed "
                           "(the game is paused while it is open).");

        // Comparison with the other mods' trough water (Realistic Water Two).
        ImGui::SeparatorText("Compare");
        const auto ours = WaterConverter::OurShapes().size();
        const auto others = WaterConverter::OtherShapes().size();
        ImGui::Text("Forge troughs with our water nearby: %zu. Other mods' water hidden in them: %zu.", ours, others);
        bool showOther = WaterConverter::showOtherWater;
        ImGui::BeginDisabled(others == 0 && !showOther);
        if (ImGui::Checkbox("Show the other mod's water instead of ours", &showOther)) {
            WaterConverter::SetCompareMode(showOther);
        }
        ImGui::EndDisabled();
        ImGui::SetItemTooltip("Realistic Water Two puts its own water in vanilla forge troughs. Switch to see it in "
                              "place of ours, then switch back.");

        // Wave texture of the forge water only: the vanilla one shipped with the plugin, or the global
        // one shared by all water (replaced by water mods such as Realistic Water Two).
        ImGui::SeparatorText("Wave texture");
        const bool vanillaWaves = Settings::waveTexture == Settings::VANILLA_WAVE_TEXTURE;
        const bool globalWaves = Settings::waveTexture.empty();
        if (ImGui::RadioButton("Vanilla waves", vanillaWaves)) {
            WaterConverter::SetWaveTexture(Settings::VANILLA_WAVE_TEXTURE);
        }
        ImGui::SetItemTooltip("The game's original wave texture, shipped with this plugin: unaffected by water mods.");
        ImGui::SameLine();
        if (ImGui::RadioButton("Installed water mod's waves", globalWaves)) {
            WaterConverter::SetWaveTexture("");
        }
        ImGui::SetItemTooltip("The global wave texture shared by every water, as replaced by your water mod "
                              "(Realistic Water Two...).");
        if (!vanillaWaves && !globalWaves) {
            ImGui::TextDisabled("Custom (from the ini): %s", Settings::waveTexture.c_str());
        }
        ImGui::TextDisabled("A new wave texture shows after loading a save (use Save to ini to keep it).");

        auto& data = water->data;
        bool changed = false;
        changed |= Group("Reflections", WaterLook::Group::kReflections, data, true);
        changed |= Group("Color and transparency", WaterLook::Group::kTransparency, data, true);
        changed |= Group("Highlights and sun", WaterLook::Group::kHighlights, data);
        changed |= Group("Wind and waves", WaterLook::Group::kWind, data);
        changed |= Group("Ripples", WaterLook::Group::kRipples, data);
        if (changed) WaterConverter::RequestRefresh();

        // INI buttons.
        static std::string status;
        ImGui::Spacing();
        ImGui::SeparatorText("Settings file");
        if (ImGui::Button("Save to ini")) {
            status = Settings::SaveLook(data) ? std::format("Saved to {}", Settings::FILE)
                                              : std::format("Could not write {}", Settings::FILE);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload from ini")) {
            if (auto task = SKSE::GetTaskInterface()) {
                task->AddTask([] {
                    Settings::Load();
                    WaterConverter::RefreshWaterType();
                    WaterConverter::RefreshOurMaterials();
                });
            }
            status = std::format("Reloaded {}", Settings::FILE);
        }
        ImGui::SameLine();
        if (ImGui::Button("Vanilla trough water")) {
            WaterConverter::ResetToSource();
            status = "Back to the vanilla horse trough water values (not saved)";
        }
        ImGui::SetItemTooltip("The values of the water type the forge water is copied from, without any change. "
                              "Very clear, with no ripples.");
        if (!status.empty()) ImGui::TextDisabled("%s", status.c_str());
    }

    inline void Register() {
        if (!SKSEMenuFramework::IsInstalled()) {
            SKSE::log::info("SKSE Menu Framework not installed: settings come from the INI only.");
            return;
        }
        // The SDK header looks the framework up when our DLL loads, but SKSE loads plugins in
        // alphabetical order and RealForgeWater comes before SKSEMenuFramework: look it up again now.
        menuFramework = GetModuleHandle(L"SKSEMenuFramework");
        if (!menuFramework) {
            SKSE::log::warn("SKSE Menu Framework is installed but not loaded: settings come from the INI only.");
            return;
        }
        SKSEMenuFramework::SetSection("Real Forge Water");
        SKSEMenuFramework::AddSectionItem("Water look", RenderWater);
        SKSE::log::info("In-game menu registered with SKSE Menu Framework.");
    }
}

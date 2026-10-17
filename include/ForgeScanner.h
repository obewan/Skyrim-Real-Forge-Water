#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "WaterConverter.h"

// Finds forges as their 3D loads, and the fake water shape in their mesh, whatever mesh a forge mod
// uses: the water shape is recognized by its texture or its name, not by a fixed mesh layout.

namespace ForgeScanner {

    using Util::Contains;
    using Util::FormatPoint;

    inline const char* GetModelPath(RE::TESBoundObject* a_base) {
        auto model = a_base->As<RE::TESModel>();
        return model ? model->GetModel() : nullptr;
    }

    // A forge is either a furniture carrying a "Crafting...Forge" keyword (CraftingSmithingForge,
    // CraftingSmithingSkyforge...), or an object whose mesh is a blacksmith forge. The second rule
    // catches forges with a built-in trough that are statics, like the Raven Rock forge.
    inline bool IsForge(RE::TESBoundObject* a_base) {
        if (auto furniture = a_base->As<RE::TESFurniture>()) {
            bool isForge = false;
            furniture->ForEachKeyword([&isForge](RE::BGSKeyword* a_keyword) {
                const auto editorID = a_keyword->GetFormEditorID();
                if (editorID && Contains(editorID, "crafting") && Contains(editorID, "forge")) {
                    isForge = true;
                    return RE::BSContainer::ForEachResult::kStop;
                }
                return RE::BSContainer::ForEachResult::kContinue;
            });
            if (isForge) return true;
        }

        auto modelPath = GetModelPath(a_base);
        return modelPath && Contains(modelPath, "blacksmithforge");
    }

    // Base objects are checked once, then the answer is cached: the 3D hooks run for every
    // reference that loads, so this must stay cheap.
    inline bool IsForgeCached(RE::TESBoundObject* a_base) {
        static std::mutex mutex;
        static std::unordered_map<RE::FormID, bool> cache;

        std::scoped_lock lock(mutex);
        auto [it, inserted] = cache.try_emplace(a_base->GetFormID(), false);
        if (inserted) {
            it->second = IsForge(a_base);
            if (it->second && Settings::verboseLog) {
                SKSE::log::info("Forge base object recognized: {:08X} \"{}\"", a_base->GetFormID(), a_base->GetName());
            }
        }
        return it->second;
    }

    inline std::string GetShaderName(RE::BSGeometry* a_geometry) {
        auto shader = a_geometry->GetGeometryRuntimeData().shaderProperty.get();
        if (!shader) return "none";
        auto rtti = shader->GetRTTI();
        return rtti ? rtti->GetName() : "unknown";
    }

    inline std::string GetDiffuseTexture(RE::BSGeometry* a_geometry) {
        auto shader = a_geometry->GetGeometryRuntimeData().shaderProperty.get();
        if (!shader || !shader->material) return {};

        if (auto lighting = netimmerse_cast<RE::BSLightingShaderProperty*>(shader)) {
            auto material = static_cast<RE::BSLightingShaderMaterialBase*>(lighting->material);
            if (material->textureSet) {
                auto path = material->textureSet->GetTexturePath(RE::BSTextureSet::Texture::kDiffuse);
                return path ? path : "";
            }
        } else if (auto effect = netimmerse_cast<RE::BSEffectShaderProperty*>(shader)) {
            auto material = static_cast<RE::BSEffectShaderMaterial*>(effect->material);
            return material->sourceTexturePath.c_str();
        }
        return {};
    }

    inline bool LooksLikeWater(RE::BSGeometry* a_geometry, const std::string& a_diffuse) {
        if (netimmerse_cast<RE::BSWaterShaderProperty*>(a_geometry->GetGeometryRuntimeData().shaderProperty.get())) {
            return true;
        }
        return Contains(a_diffuse, "water") || Contains(a_geometry->name.c_str(), "water");
    }

    // Main thread only.
    inline void ScanForge(RE::TESObjectREFR* a_ref) {
        auto base = a_ref->GetBaseObject();
        auto root = a_ref->Get3D();
        if (!base || !root || !WaterConverter::MarkScanned(a_ref, root)) return;

        auto modelPath = GetModelPath(base);
        SKSE::log::info("Forge {:08X} \"{}\" ({})", a_ref->GetFormID(), base->GetName(), modelPath ? modelPath : "");

        // Collect first, convert after: converting changes the scene graph being traversed.
        int shapeCount = 0;
        std::vector<RE::BSGeometry*> waterShapes;
        RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* a_geometry) {
            // Shapes without a shader are invisible markers (the furniture skeleton, triggers...).
            if (!a_geometry->GetGeometryRuntimeData().shaderProperty) return RE::BSVisit::BSVisitControl::kContinue;

            ++shapeCount;
            const auto diffuse = GetDiffuseTexture(a_geometry);
            const bool isWater = LooksLikeWater(a_geometry, diffuse);
            if (isWater) waterShapes.push_back(a_geometry);

            if (Settings::verboseLog) {
                const auto& bound = a_geometry->worldBound;
                SKSE::log::info("    {} shape \"{}\" shader={} diffuse=\"{}\" center={} radius={:.1f}{}",
                                isWater ? "[WATER]" : "       ", a_geometry->name.c_str(), GetShaderName(a_geometry),
                                diffuse, FormatPoint(bound.center), bound.radius,
                                a_geometry->GetAppCulled() ? " (hidden)" : "");
            }
            return RE::BSVisit::BSVisitControl::kContinue;
        });

        if (waterShapes.empty()) {
            SKSE::log::warn("    no water shape found among {} shapes (set bVerboseLog = 1 to list them)", shapeCount);
            return;
        }

        if (Settings::mode == 1) {
            for (auto geometry : waterShapes) {
                WaterConverter::Convert(a_ref, geometry);
            }
            WaterConverter::HideOtherTroughWater();
            WaterConverter::ScheduleHideOtherTroughWater();
        }
    }

    // Called from the 3D hooks, possibly while the game is still loading the cell: only
    // filter here, and do the actual scan later on the main thread.
    inline void OnReference3DLoaded(RE::TESObjectREFR* a_ref) {
        auto base = a_ref->GetBaseObject();
        if (!base) return;

        // A water activator loading after a forge may be another mod's trough water (Realistic Water Two).
        if (auto activator = base->As<RE::TESObjectACTI>(); activator && activator->waterForm) {
            WaterConverter::ScheduleHideOtherTroughWater();
            return;
        }

        if (!IsForgeCached(base)) return;

        if (auto task = SKSE::GetTaskInterface()) {
            task->AddTask([handle = a_ref->GetHandle()]() {
                if (auto ref = handle.get()) {
                    ScanForge(ref.get());
                }
            });
        }
    }

    // Second detection path: when a cell finishes loading, look at every forge in it. It catches
    // forges whose 3D was already loaded when the hooks could have seen it (a save loaded in place).
    class CellLoadedHandler : public RE::BSTEventSink<RE::TESCellFullyLoadedEvent> {
    public:
        RE::BSEventNotifyControl ProcessEvent(const RE::TESCellFullyLoadedEvent* a_event,
                                              RE::BSTEventSource<RE::TESCellFullyLoadedEvent>*) override {
            if (!a_event || !a_event->cell) return RE::BSEventNotifyControl::kContinue;

            a_event->cell->ForEachReference([](RE::TESObjectREFR* a_ref) {
                auto base = a_ref ? a_ref->GetBaseObject() : nullptr;
                if (base && IsForgeCached(base)) {
                    ScanForge(a_ref);
                }
                return RE::BSContainer::ForEachResult::kContinue;
            });
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    inline void RegisterCellLoadedHandler() {
        static CellLoadedHandler handler;
        if (auto holder = RE::ScriptEventSourceHolder::GetSingleton()) {
            holder->AddEventSink<RE::TESCellFullyLoadedEvent>(&handler);
        } else {
            SKSE::log::error("Failed to get ScriptEventSourceHolder singleton");
        }
    }
}

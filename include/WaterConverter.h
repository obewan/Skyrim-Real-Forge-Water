#pragma once

#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "Settings.h"
#include "Util.h"

// Turns the fake water plane of a forge into real engine water.
//
// The forge mesh keeps its own water shape (so size and position always match the trough), but
// that shape gets the real water shader, taken from the vanilla horse trough mesh, and is then
// registered with the engine's water system, which drives its reflections, ripples and wind.
// ENB and Community Shaders then treat it like any other water.
//
// The water system requires every water shape to be the child of a BSMultiBoundNode, whose bounding
// box it fills in (TESWaterSystem::AddWater reads parent->multiBound->data and crashes otherwise).
// So the forge water shape is moved under a copy of the trough's own water node.

namespace WaterConverter {

    // Mesh whose real water provides the water shader and its BSMultiBoundNode.
    inline constexpr auto TEMPLATE_MODEL = "Clutter\\HorseTrough\\HorseTrough01.nif";

    struct Template {
        RE::NiPointer<RE::NiNode> root;      // keeps the loaded mesh alive
        RE::BSMultiBoundNode* waterNode{};  // parent of the real water shape, inside root
    };

    inline Template* GetTemplate() {
        static Template tmpl;
        static bool loaded = false;
        if (loaded) return tmpl.waterNode ? &tmpl : nullptr;
        loaded = true;

        RE::BSModelDB::DBTraits::ArgsType args{};
        if (RE::BSModelDB::Demand(TEMPLATE_MODEL, tmpl.root, args) != RE::BSResource::ErrorCode::kNone || !tmpl.root) {
            SKSE::log::error("Could not load {}, water conversion disabled", TEMPLATE_MODEL);
            return nullptr;
        }

        RE::BSVisit::TraverseScenegraphGeometries(tmpl.root.get(), [](RE::BSGeometry* a_geometry) {
            if (netimmerse_cast<RE::BSWaterShaderProperty*>(a_geometry->GetGeometryRuntimeData().shaderProperty.get())) {
                tmpl.waterNode = netimmerse_cast<RE::BSMultiBoundNode*>(a_geometry->parent);
                return RE::BSVisit::BSVisitControl::kStop;
            }
            return RE::BSVisit::BSVisitControl::kContinue;
        });

        if (!tmpl.waterNode || !tmpl.waterNode->GetRuntimeData().multiBound) {
            SKSE::log::error("No water under a multi-bound node in {}, water conversion disabled", TEMPLATE_MODEL);
            tmpl.waterNode = nullptr;
            return nullptr;
        }
        SKSE::log::info("Water template loaded from {}", TEMPLATE_MODEL);
        return &tmpl;
    }

    // The forge water type: a copy, made in memory, of the source water type with the [WaterLook]
    // overrides applied. No plugin file needed, and the source (real horse troughs) is left untouched.
    // The game builds the water material from these values each time water is registered
    // (TESWaterSystem::AddWater), so a copy gets its own look.
    inline RE::TESWaterForm* forgeWater = nullptr;

    inline void CopyWaterType(RE::TESWaterForm* a_dst, const RE::TESWaterForm* a_src) {
        a_dst->fullName = a_src->fullName;
        a_dst->needUpdate = true;
        std::ranges::copy(a_src->texScroll, a_dst->texScroll);
        for (std::size_t i = 0; i < std::size(a_src->noiseTextures); ++i) {
            a_dst->noiseTextures[i].textureName = a_src->noiseTextures[i].textureName;
        }
        a_dst->alpha = a_src->alpha;
        a_dst->flags = a_src->flags;
        a_dst->materialType = a_src->materialType;
        a_dst->waterSound = a_src->waterSound;
        a_dst->data = a_src->data;
        std::ranges::copy(a_src->waterWeatherControl, a_dst->waterWeatherControl);
        std::ranges::copy(a_src->currentTextureSelect, a_dst->currentTextureSelect);
        a_dst->frequencyX = a_src->frequencyX;
        a_dst->frequencyY = a_src->frequencyY;
        a_dst->octaves = a_src->octaves;
        a_dst->amplitude = a_src->amplitude;
        a_dst->lacunarity = a_src->lacunarity;
        a_dst->bias = a_src->bias;
        a_dst->gain = a_src->gain;
        a_dst->contactSpell = a_src->contactSpell;
        std::ranges::copy(a_src->noiseTextureData, a_dst->noiseTextureData);
        a_dst->placeableAutoWater = a_src->placeableAutoWater;
        a_dst->placeableLODWater = a_src->placeableLODWater;
        a_dst->waterShaderMaterial = a_src->waterShaderMaterial;
        a_dst->resetNoiseTextures = a_src->resetNoiseTextures;
        a_dst->imageSpace = a_src->imageSpace;
        a_dst->linearVelocity = a_src->linearVelocity;
        a_dst->angularVelocity = a_src->angularVelocity;
    }

    // The water type the forge water type was copied from (vanilla horse trough water by default).
    inline RE::TESWaterForm* sourceWater = nullptr;

    inline void ApplyLook(RE::WaterShaderData& a_data) {
        for (const auto& field : WaterLook::FLOATS) {
            if (auto it = Settings::floatOverrides.find(field.key); it != Settings::floatOverrides.end()) {
                field.get(a_data) = it->second;
            }
        }
        for (const auto& field : WaterLook::COLORS) {
            if (auto it = Settings::colorOverrides.find(field.key); it != Settings::colorOverrides.end()) {
                field.get(a_data) = it->second;
            }
        }
    }

    // Our own wave texture for the 3 noise layers (NAM2-NAM4), so the forge water can differ from the
    // global one without touching any other water. The 4th slot (NAM5) is the flow map. An empty
    // setting goes back to the source water type's textures (none for the vanilla trough water, which
    // then uses the global wave texture).
    inline void ApplyWaveTexture() {
        if (!forgeWater || !sourceWater) return;
        for (std::size_t i = 0; i < 3; ++i) {
            forgeWater->noiseTextures[i].textureName = Settings::waveTexture.empty()
                                                           ? sourceWater->noiseTextures[i].textureName
                                                           : RE::BSFixedString(Settings::waveTexture.c_str());
            forgeWater->noiseTextureData[i].reset();
        }
        forgeWater->resetNoiseTextures = true;
    }

    // (Re)builds the forge water type from the source water type and the current settings.
    // Main thread only.
    inline void RefreshWaterType() {
        RE::TESWaterForm* source = nullptr;
        if (auto dataHandler = RE::TESDataHandler::GetSingleton()) {
            source = dataHandler->LookupForm<RE::TESWaterForm>(Settings::waterTypeID & 0xFFFFFF, Settings::waterTypePlugin);
        }
        if (!source) {
            SKSE::log::error("Water type {:06X} not found in {}", Settings::waterTypeID & 0xFFFFFF,
                             Settings::waterTypePlugin);
            return;
        }

        if (!forgeWater) {
            forgeWater = RE::IFormFactory::Create<RE::TESWaterForm>();
            if (!forgeWater) {
                SKSE::log::error("Could not create the forge water type");
                return;
            }
        }
        sourceWater = source;
        CopyWaterType(forgeWater, source);
        ApplyLook(forgeWater->data);

        ApplyWaveTexture();

        const auto& d = forgeWater->data;
        SKSE::log::info("Forge water type {:08X}, copied from {:08X} \"{}\":", forgeWater->GetFormID(),
                        source->GetFormID(), source->GetFormEditorID());
        SKSE::log::info("    reflection amount {:.2f}, fresnel {:.2f}, reflection magnitude {:.2f}, depth reflections {:.2f}",
                        d.reflectionAmount, d.fresnelAmount, d.reflectionMagnitude, d.depthProperties.reflections);
        SKSE::log::info("    fog near {:.0f}, far {:.0f}, amount {:.2f}, refraction {:.1f}", d.aboveWaterFogDistNear,
                        d.aboveWaterFogDistFar, d.aboveWaterFogAmount, d.refractionMagnitude);
        SKSE::log::info("    wave textures \"{}\", \"{}\", \"{}\"", forgeWater->noiseTextures[0].textureName.c_str(),
                        forgeWater->noiseTextures[1].textureName.c_str(), forgeWater->noiseTextures[2].textureName.c_str());
        SKSE::log::info("    shallow ({}, {}, {}), deep ({}, {}, {}), reflection ({}, {}, {})", d.shallowWaterColor.red,
                        d.shallowWaterColor.green, d.shallowWaterColor.blue, d.deepWaterColor.red,
                        d.deepWaterColor.green, d.deepWaterColor.blue, d.reflectionWaterColor.red,
                        d.reflectionWaterColor.green, d.reflectionWaterColor.blue);
    }

    inline RE::TESWaterForm* GetWaterType() {
        if (!forgeWater) RefreshWaterType();
        return forgeWater;
    }

    // Converted shapes per forge reference, so they can be unregistered before the forge 3D goes away,
    // and the 3D root already handled per forge reference, so several detection paths convert it once.
    inline std::mutex convertedMutex;
    inline std::unordered_map<RE::FormID, std::vector<RE::NiPointer<RE::BSGeometry>>> converted;
    inline std::unordered_map<RE::FormID, RE::NiAVObject*> scannedRoots;

    // Other mods' water hidden in our troughs, per forge reference, kept to show it again in the
    // menu's comparison mode.
    struct HiddenWater {
        RE::NiPointer<RE::BSTriShape> shape;
        RE::TESWaterForm* type{};
        RE::FormID owner{};
        // Other shapes of the same object in the trough, such as Realistic Water Two's dirt flecks
        // floating on its water: hidden and shown together with the water.
        std::vector<RE::NiPointer<RE::BSGeometry>> extras;

        void SetHidden(bool a_hidden) const {
            shape->SetAppCulled(a_hidden);
            for (auto& extra : extras) extra->SetAppCulled(a_hidden);
        }
    };

    // Shapes of the other water's object lying in the same trough as its water.
    inline std::vector<RE::NiPointer<RE::BSGeometry>> FindExtras(RE::BSTriShape* a_water) {
        std::vector<RE::NiPointer<RE::BSGeometry>> extras;
        auto owner = a_water->GetUserData();
        auto root = owner ? owner->Get3D() : nullptr;
        if (!root) return extras;

        const auto& water = a_water->worldBound;
        RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* a_geometry) {
            // Only surface films (effect shaders) and extra water: never solid parts such as a trough.
            auto shader = a_geometry->GetGeometryRuntimeData().shaderProperty.get();
            const bool isFilm = netimmerse_cast<RE::BSEffectShaderProperty*>(shader) ||
                                netimmerse_cast<RE::BSWaterShaderProperty*>(shader);
            const auto& bound = a_geometry->worldBound;
            if (isFilm && a_geometry != a_water && !a_geometry->GetAppCulled() && Util::InSameTrough(bound, water) &&
                bound.radius < water.radius * 2.0f) {
                extras.emplace_back(a_geometry);
            }
            return RE::BSVisit::BSVisitControl::kContinue;
        });
        return extras;
    }
    inline std::unordered_map<RE::FormID, std::vector<HiddenWater>> hiddenOthers;

    // Menu comparison mode: show the other mods' water instead of ours.
    inline std::atomic_bool showOtherWater{false};

    // The game reads the bounds list, so pass an empty one rather than nullptr.
    inline const RE::BSTArray<RE::NiPointer<RE::BSMultiBoundAABB>> noBounds;

    inline void Register(RE::BSGeometry* a_shape, RE::TESWaterForm* a_type) {
        if (auto waterSystem = RE::TESWaterSystem::GetSingleton()) {
            waterSystem->AddWater(a_shape, a_type, a_shape->worldBound.center.z, &noBounds, false, false);
        }
    }

    inline void Unregister(RE::BSGeometry* a_shape) {
        if (auto waterSystem = RE::TESWaterSystem::GetSingleton()) {
            waterSystem->RemoveWater(a_shape);
        }
    }

    // Copies of the lists, for main thread work that must not hold the lock while calling the game.
    inline std::vector<RE::NiPointer<RE::BSGeometry>> OurShapes() {
        std::scoped_lock lock(convertedMutex);
        std::vector<RE::NiPointer<RE::BSGeometry>> shapes;
        for (auto& [refID, list] : converted) shapes.insert(shapes.end(), list.begin(), list.end());
        return shapes;
    }

    inline std::vector<HiddenWater> OtherShapes() {
        std::scoped_lock lock(convertedMutex);
        std::vector<HiddenWater> shapes;
        for (auto& [refID, list] : hiddenOthers) shapes.insert(shapes.end(), list.begin(), list.end());
        return shapes;
    }

    // Returns false when this exact 3D was already handled.
    inline bool MarkScanned(RE::TESObjectREFR* a_ref, RE::NiAVObject* a_root) {
        std::scoped_lock lock(convertedMutex);
        auto [it, inserted] = scannedRoots.try_emplace(a_ref->GetFormID(), a_root);
        if (!inserted && it->second == a_root) return false;
        it->second = a_root;
        return true;
    }

    inline void Release(RE::FormID a_refID) {
        std::scoped_lock lock(convertedMutex);
        scannedRoots.erase(a_refID);
        hiddenOthers.erase(a_refID);
        auto it = converted.find(a_refID);
        if (it == converted.end()) return;

        auto waterSystem = RE::TESWaterSystem::GetSingleton();
        for (auto& geometry : it->second) {
            if (waterSystem) waterSystem->RemoveWater(geometry.get());
        }
        if (Settings::verboseLog) {
            SKSE::log::info("Forge {:08X}: released {} water shape(s)", a_refID, it->second.size());
        }
        converted.erase(it);
    }

    // Main thread only.
    inline bool Convert(RE::TESObjectREFR* a_ref, RE::BSGeometry* a_geometry) {
        auto tmpl = GetTemplate();
        auto waterType = GetWaterType();
        auto waterSystem = RE::TESWaterSystem::GetSingleton();
        if (!tmpl || !waterType || !waterSystem) return false;

        auto parent = a_geometry->parent;
        if (!parent) return false;

        if (!netimmerse_cast<RE::BSMultiBoundNode*>(parent)) {
            // A copy of the trough water node comes with its own bounding box, and with a copy of the
            // trough water shape, whose water shader we take before removing that shape.
            RE::NiPointer<RE::NiObject> clone{tmpl->waterNode->Clone()};
            RE::NiPointer<RE::BSMultiBoundNode> boundNode{netimmerse_cast<RE::BSMultiBoundNode*>(clone.get())};
            if (!boundNode || !boundNode->GetRuntimeData().multiBound ||
                boundNode->GetRuntimeData().multiBound == tmpl->waterNode->GetRuntimeData().multiBound) {
                SKSE::log::error("    the copied water node has no bounding box of its own, skipped");
                return false;
            }

            RE::NiPointer<RE::BSShaderProperty> shader;
            RE::NiPointer<RE::NiAlphaProperty> alpha;
            auto& children = boundNode->GetChildren();
            for (auto i = children.capacity(); i-- > 0;) {
                if (auto child = children[i].get()) {
                    if (auto geometry = child->AsGeometry(); geometry && !shader) {
                        shader = geometry->GetGeometryRuntimeData().shaderProperty;
                        alpha = geometry->GetGeometryRuntimeData().alphaProperty;
                    }
                    boundNode->DetachChildAt(i);
                }
            }
            if (!shader) {
                SKSE::log::error("    the copied water node has no water shader, skipped");
                return false;
            }

            // A forge mesh may already ship a real water shader on its water: keep that one. It still
            // needs registering, since the game registers water for water activators only.
            auto& data = a_geometry->GetGeometryRuntimeData();
            if (!netimmerse_cast<RE::BSWaterShaderProperty*>(data.shaderProperty.get())) {
                data.shaderProperty = shader;
                data.alphaProperty = alpha;
            }

            // The bounding box lies in the node's space, around the shape's vertices, without the
            // shape's own transform. Forge water shapes sit far from their parent's origin (236 units
            // in the Whiterun forge mesh): left in the shape, that offset puts the box at the forge
            // center, and the water disappears whenever that point is off screen. So the node takes
            // the shape's transform, and the shape keeps exactly the same place in the world.
            boundNode->local = a_geometry->local;
            a_geometry->local = RE::NiTransform{};
            RE::NiPointer<RE::NiAVObject> keepAlive{a_geometry};
            parent->DetachChild(a_geometry);
            boundNode->AttachChild(a_geometry);
            parent->AttachChild(boundNode.get());

            RE::NiUpdateData updateData{};
            boundNode->Update(updateData);
        }

        const float height = a_geometry->worldBound.center.z;
        if (showOtherWater) {
            a_geometry->SetAppCulled(true);  // menu comparison mode: ours stays hidden for now
        } else {
            Register(a_geometry, waterType);
        }

        {
            std::scoped_lock lock(convertedMutex);
            converted[a_ref->GetFormID()].emplace_back(a_geometry);
        }
        SKSE::log::info("    converted \"{}\" to real water at height {:.1f}", a_geometry->name.c_str(), height);
        return true;
    }

    // Other mods can put their own real water in forge troughs: Realistic Water Two places a water
    // activator at every vanilla forge. Its surface sits right on top of ours and hides it, so any
    // other water found in a converted trough is unregistered and hidden. Main thread only.
    inline void HideOtherTroughWater() {
        auto waterSystem = RE::TESWaterSystem::GetSingleton();
        if (!Settings::hideOtherTroughWater || showOtherWater || !waterSystem) return;

        std::vector<std::pair<RE::FormID, RE::NiPointer<RE::BSGeometry>>> ours;
        {
            std::scoped_lock lock(convertedMutex);
            for (auto& [refID, shapes] : converted) {
                for (auto& shape : shapes) ours.emplace_back(refID, shape);
            }
        }
        if (ours.empty()) return;

        // Collect first: RemoveWater changes the list being iterated.
        std::vector<std::pair<RE::FormID, HiddenWater>> others;
        for (auto& object : waterSystem->waterObjects) {
            auto shape = object ? object->shape.get() : nullptr;
            if (!shape || std::ranges::any_of(ours, [&](auto& a_ours) { return a_ours.second.get() == shape; })) continue;

            const auto& theirs = shape->worldBound;
            for (auto& [forgeID, geometry] : ours) {
                const auto& mine = geometry->worldBound;
                if (Util::InSameTrough(mine, theirs) && theirs.radius < mine.radius * 2.0f) {
                    auto owner = shape->GetUserData();
                    others.emplace_back(forgeID, HiddenWater{object->shape, object->waterType, owner ? owner->GetFormID() : 0});
                    break;
                }
            }
        }

        for (auto& [forgeID, other] : others) {
            other.extras = FindExtras(other.shape.get());
            SKSE::log::info("Hiding other water \"{}\" of ref {:08X} found in a converted forge trough, with {} "
                            "other shape(s) of it",
                            other.shape->name.c_str(), other.owner, other.extras.size());
            waterSystem->RemoveWater(other.shape.get());
            other.SetHidden(true);
            std::scoped_lock lock(convertedMutex);
            hiddenOthers[forgeID].push_back(other);
        }
    }

    // Live look changes: the game builds the water material when water is registered, so register
    // our shapes again. Main thread only.
    inline void ReRegisterOurs() {
        if (showOtherWater || !forgeWater) return;
        for (auto& shape : OurShapes()) {
            Unregister(shape.get());
            Register(shape.get(), forgeWater);
        }
    }

    // Live look changes without re-registering: re-registering water after its cell has loaded loses
    // its reflections. Instead, refill the material our shapes already use from the forge water type,
    // with the function TESWaterSystem::AddWater uses to build it (TESWaterForm -> BSWaterShaderMaterial).
    // Only its AE address is known: on other runtimes, fall back to re-registering. Main thread only.
    inline void RefreshOurMaterials() {
        if (showOtherWater || !forgeWater) return;
        if (!REL::Module::IsAE()) {
            ReRegisterOurs();
            return;
        }

        using FillMaterial_t = void (*)(RE::TESWaterForm*, RE::BSWaterShaderMaterial*);
        static REL::Relocation<FillMaterial_t> fillMaterial{REL::ID(20373)};
        for (auto& shape : OurShapes()) {
            auto shader = shape->GetGeometryRuntimeData().shaderProperty.get();
            auto material = shader ? shader->material : nullptr;
            if (material && material->GetType() == RE::BSShaderMaterial::Type::kWater) {
                fillMaterial(forgeWater, static_cast<RE::BSWaterShaderMaterial*>(material));
            }
        }
    }

    // From the menu (render thread): at most one refresh per frame.
    inline void RequestRefresh() {
        static std::atomic_bool pending{false};
        if (pending.exchange(true)) return;
        if (auto task = SKSE::GetTaskInterface()) {
            task->AddTask([] {
                pending = false;
                RefreshOurMaterials();
            });
        } else {
            pending = false;
        }
    }

    // Menu comparison mode: swap between our water and the other mods' water hidden in our troughs.
    // Main thread only.
    inline void ApplyCompareMode() {
        const auto ours = OurShapes();
        const auto others = OtherShapes();
        if (showOtherWater) {
            for (auto& shape : ours) {
                Unregister(shape.get());
                shape->SetAppCulled(true);
            }
            for (auto& other : others) {
                other.SetHidden(false);
                Register(other.shape.get(), other.type);
            }
        } else {
            for (auto& other : others) {
                Unregister(other.shape.get());
                other.SetHidden(true);
            }
            for (auto& shape : ours) {
                shape->SetAppCulled(false);
                Unregister(shape.get());
                Register(shape.get(), forgeWater);
            }
            HideOtherTroughWater();
        }
        SKSE::log::info("Comparison mode: showing {} ({} of ours, {} others)",
                        showOtherWater ? "the other mods' water" : "our water", ours.size(), others.size());
    }

    inline void SetCompareMode(bool a_showOther) {
        showOtherWater = a_showOther;
        if (auto task = SKSE::GetTaskInterface()) task->AddTask([] { ApplyCompareMode(); });
    }

    // From the menu: switch the wave texture. It shows on the next save load: textures are only loaded
    // when the water is registered, and registering it again now would lose its reflections.
    inline void SetWaveTexture(std::string a_path) {
        if (auto task = SKSE::GetTaskInterface()) {
            task->AddTask([path = std::move(a_path)] {
                Settings::waveTexture = path;
                ApplyWaveTexture();
            });
        }
    }

    // Back to the source water type's own values (the vanilla trough water), without the INI overrides.
    inline void ResetToSource() {
        if (forgeWater && sourceWater) {
            forgeWater->data = sourceWater->data;
            RequestRefresh();
        }
    }

    // The other water may register after our conversion: check again a little later.
    inline void ScheduleHideOtherTroughWater() {
        static std::atomic_bool scheduled{false};
        if (scheduled.exchange(true)) return;

        std::thread([] {
            for (int seconds : {1, 4}) {
                std::this_thread::sleep_for(std::chrono::seconds(seconds));
                if (auto task = SKSE::GetTaskInterface()) {
                    task->AddTask([] { HideOtherTroughWater(); });
                }
            }
            scheduled = false;
        }).detach();
    }

    class CellDetachHandler : public RE::BSTEventSink<RE::TESCellAttachDetachEvent> {
    public:
        RE::BSEventNotifyControl ProcessEvent(const RE::TESCellAttachDetachEvent* a_event,
                                              RE::BSTEventSource<RE::TESCellAttachDetachEvent>*) override {
            if (a_event && a_event->reference && !a_event->attached) {
                Release(a_event->reference->GetFormID());
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    inline void RegisterCellDetachHandler() {
        static CellDetachHandler handler;
        if (auto holder = RE::ScriptEventSourceHolder::GetSingleton()) {
            holder->AddEventSink<RE::TESCellAttachDetachEvent>(&handler);
        }
    }
}

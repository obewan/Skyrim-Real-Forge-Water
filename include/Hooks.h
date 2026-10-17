#pragma once

#include "ForgeScanner.h"

namespace Hooks {

    // TESObjectREFR::Load3D (vfunc 0x6A): builds the 3D of a plain reference (furniture, static,
    // activator...). With background loading it can return nullptr and attach the 3D later.
    struct Load3D {
        static RE::NiAVObject* thunk(RE::TESObjectREFR* a_this, bool a_backgroundLoading) {
            auto result = func(a_this, a_backgroundLoading);
            if (a_this && result) {
                ForgeScanner::OnReference3DLoaded(a_this);
            }
            return result;
        }
        static inline REL::Relocation<decltype(thunk)> func;
    };

    // TESObjectREFR::Set3D (vfunc 0x6C): attaches or drops the 3D of a reference.
    struct Set3D {
        static void thunk(RE::TESObjectREFR* a_this, RE::NiAVObject* a_object, bool a_queue3DTasks) {
            // The current 3D is about to be replaced or dropped: unregister its converted water first.
            if (a_this && a_this->Get3D() != a_object) {
                WaterConverter::Release(a_this->GetFormID());
            }
            func(a_this, a_object, a_queue3DTasks);
            if (a_this && a_object) {
                ForgeScanner::OnReference3DLoaded(a_this);
            }
        }
        static inline REL::Relocation<decltype(thunk)> func;
    };

    inline void Install() {
        REL::Relocation<std::uintptr_t> vtable{RE::VTABLE_TESObjectREFR[0]};
        Load3D::func = vtable.write_vfunc(0x6A, Load3D::thunk);
        Set3D::func = vtable.write_vfunc(0x6C, Set3D::thunk);
        SKSE::log::info("Load3D and Set3D hooks installed");
    }
}

#pragma once

#include <array>

// Every water type setting the plugin exposes, shared by the INI ([WaterLook]) and the in-game menu.
// They all live in the water type's shader data (WATR DNAM).

namespace WaterLook {

    enum class Group { kReflections, kTransparency, kHighlights, kWind, kRipples };

    struct FloatField {
        const char* key;    // INI key in [WaterLook]
        const char* label;  // menu label
        Group group;
        float min;
        float max;
        const char* format;
        float& (*get)(RE::WaterShaderData&);
        const char* tooltip;
    };

    struct ColorField {
        const char* key;
        const char* label;
        Group group;
        RE::Color& (*get)(RE::WaterShaderData&);
    };

    using D = RE::WaterShaderData;

    // clang-format off
    inline const std::array FLOATS{
        FloatField{"fReflectionAmount", "Reflection amount", Group::kReflections, 0.0f, 1.0f, "%.2f",
            [](D& d) -> float& { return d.reflectionAmount; }, "How much the surface reflects the sky and surroundings."},
        FloatField{"fFresnelAmount", "Fresnel", Group::kReflections, 0.0f, 1.0f, "%.2f",
            [](D& d) -> float& { return d.fresnelAmount; }, "Extra reflection when looking at the water at a grazing angle."},
        FloatField{"fReflectionMagnitude", "Reflection strength", Group::kReflections, 0.0f, 4.0f, "%.2f",
            [](D& d) -> float& { return d.reflectionMagnitude; }, "Brightness of what is reflected."},
        FloatField{"fDepthReflections", "Reflections in shallow water", Group::kReflections, 0.0f, 1.0f, "%.2f",
            [](D& d) -> float& { return d.depthProperties.reflections; }, "How much reflection shallow water keeps. A trough is all shallow water."},

        FloatField{"fFogDistanceNear", "Color starts at depth", Group::kTransparency, 0.0f, 300.0f, "%.0f",
            [](D& d) -> float& { return d.aboveWaterFogDistNear; }, "Depth, in game units, where the water color starts. The trough is about 25 units deep."},
        FloatField{"fFogDistanceFar", "Opaque at depth", Group::kTransparency, 1.0f, 1000.0f, "%.0f",
            [](D& d) -> float& { return d.aboveWaterFogDistFar; }, "Depth where the water color is complete. Lower = less see-through."},
        FloatField{"fFogAmount", "Color opacity", Group::kTransparency, 0.0f, 1.0f, "%.2f",
            [](D& d) -> float& { return d.aboveWaterFogAmount; }, "How opaque the water color gets."},
        FloatField{"fRefractionMagnitude", "Refraction", Group::kTransparency, 0.0f, 100.0f, "%.0f",
            [](D& d) -> float& { return d.refractionMagnitude; }, "Distortion of what is seen through the water."},
        FloatField{"fDepthRefraction", "Refraction in shallow water", Group::kTransparency, 0.0f, 1.0f, "%.2f",
            [](D& d) -> float& { return d.depthProperties.refraction; }, nullptr},

        FloatField{"fSpecularBrightness", "Highlights brightness", Group::kHighlights, 0.0f, 200.0f, "%.0f",
            [](D& d) -> float& { return d.specularBrightness; }, "Highlights from lights such as the forge fire."},
        FloatField{"fSpecularPower", "Highlights sharpness", Group::kHighlights, 1.0f, 10000.0f, "%.0f",
            [](D& d) -> float& { return d.specularPower; }, nullptr},
        FloatField{"fDepthSpecular", "Highlights in shallow water", Group::kHighlights, 0.0f, 1.0f, "%.2f",
            [](D& d) -> float& { return d.depthProperties.specularLighting; }, nullptr},
        FloatField{"fSunSpecularMagnitude", "Sun highlight", Group::kHighlights, 0.0f, 10.0f, "%.2f",
            [](D& d) -> float& { return d.sunSpecularMagnitude; }, nullptr},
        FloatField{"fSunSpecularPower", "Sun highlight sharpness", Group::kHighlights, 1.0f, 10000.0f, "%.0f",
            [](D& d) -> float& { return d.sunSpecularPower; }, nullptr},
        FloatField{"fSunSparkleMagnitude", "Sun sparkles", Group::kHighlights, 0.0f, 10.0f, "%.2f",
            [](D& d) -> float& { return d.sunSparkleMagnitude; }, nullptr},
        FloatField{"fSunSparklePower", "Sun sparkles sharpness", Group::kHighlights, 1.0f, 10000.0f, "%.0f",
            [](D& d) -> float& { return d.sunSparklePower; }, nullptr},

        FloatField{"fWindSpeed1", "Layer 1 speed", Group::kWind, 0.0f, 0.3f, "%.3f",
            [](D& d) -> float& { return d.noiseWindSpeedA[0]; }, "The surface is animated by 3 noise layers moving with the wind."},
        FloatField{"fWindDirection1", "Layer 1 direction", Group::kWind, 0.0f, 360.0f, "%.0f",
            [](D& d) -> float& { return d.noiseWindDirectionA[0]; }, nullptr},
        FloatField{"fWaveAmplitude1", "Layer 1 amplitude", Group::kWind, 0.0f, 2.0f, "%.2f",
            [](D& d) -> float& { return d.amplitudeA[0]; }, "Height of the small waves of this layer."},
        FloatField{"fWaveScale1", "Layer 1 scale", Group::kWind, 10.0f, 8000.0f, "%.0f",
            [](D& d) -> float& { return d.uvScaleA[0]; }, "Size of the waves of this layer. Lower = smaller, denser waves."},
        FloatField{"fWindSpeed2", "Layer 2 speed", Group::kWind, 0.0f, 0.3f, "%.3f",
            [](D& d) -> float& { return d.noiseWindSpeedA[1]; }, nullptr},
        FloatField{"fWindDirection2", "Layer 2 direction", Group::kWind, 0.0f, 360.0f, "%.0f",
            [](D& d) -> float& { return d.noiseWindDirectionA[1]; }, nullptr},
        FloatField{"fWaveAmplitude2", "Layer 2 amplitude", Group::kWind, 0.0f, 2.0f, "%.2f",
            [](D& d) -> float& { return d.amplitudeA[1]; }, nullptr},
        FloatField{"fWaveScale2", "Layer 2 scale", Group::kWind, 10.0f, 8000.0f, "%.0f",
            [](D& d) -> float& { return d.uvScaleA[1]; }, nullptr},
        FloatField{"fWindSpeed3", "Layer 3 speed", Group::kWind, 0.0f, 0.3f, "%.3f",
            [](D& d) -> float& { return d.noiseWindSpeedA[2]; }, nullptr},
        FloatField{"fWindDirection3", "Layer 3 direction", Group::kWind, 0.0f, 360.0f, "%.0f",
            [](D& d) -> float& { return d.noiseWindDirectionA[2]; }, nullptr},
        FloatField{"fWaveAmplitude3", "Layer 3 amplitude", Group::kWind, 0.0f, 2.0f, "%.2f",
            [](D& d) -> float& { return d.amplitudeA[2]; }, nullptr},
        FloatField{"fWaveScale3", "Layer 3 scale", Group::kWind, 10.0f, 8000.0f, "%.0f",
            [](D& d) -> float& { return d.uvScaleA[2]; }, nullptr},
        FloatField{"fDepthNormals", "Waves in shallow water", Group::kWind, 0.0f, 1.0f, "%.2f",
            [](D& d) -> float& { return d.depthProperties.normals; }, "How much of the waves shallow water keeps."},

        FloatField{"fRippleSize", "Ripple size", Group::kRipples, 0.0f, 2.0f, "%.2f",
            [](D& d) -> float& { return d.displacementSize; }, "Ripples from rain, hits and anything entering the water. All 0 = no ripples (vanilla trough water)."},
        FloatField{"fRippleForce", "Ripple force", Group::kRipples, 0.0f, 1.0f, "%.2f",
            [](D& d) -> float& { return d.displacementForce; }, nullptr},
        FloatField{"fRippleVelocity", "Ripple speed", Group::kRipples, 0.0f, 2.0f, "%.2f",
            [](D& d) -> float& { return d.displacementVelocity; }, nullptr},
        FloatField{"fRippleFalloff", "Ripple falloff", Group::kRipples, 0.0f, 1.0f, "%.2f",
            [](D& d) -> float& { return d.displacementFalloff; }, nullptr},
        FloatField{"fRippleDampener", "Ripple damping", Group::kRipples, 0.0f, 1.0f, "%.3f",
            [](D& d) -> float& { return d.displacementDampener; }, "How long ripples last. Close to 1 = longer."},
    };

    inline const std::array COLORS{
        ColorField{"sShallowColor", "Shallow color", Group::kTransparency, [](D& d) -> RE::Color& { return d.shallowWaterColor; }},
        ColorField{"sDeepColor", "Deep color", Group::kTransparency, [](D& d) -> RE::Color& { return d.deepWaterColor; }},
        ColorField{"sReflectionColor", "Reflection tint", Group::kReflections, [](D& d) -> RE::Color& { return d.reflectionWaterColor; }},
    };
    // clang-format on
}

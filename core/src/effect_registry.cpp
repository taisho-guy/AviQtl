#include "effect_registry.hpp"

namespace Rina::Core {

void initializeStandardEffects() {
    auto& reg = EffectRegistry::instance();

    reg.registerEffect({
        "blur", "ぼかし", "",
        {{"size", 10}, {"aspect", 0}}
    });

    reg.registerEffect({
        "color_correction", "色調補正", "",
        {{"brightness", 100}, {"contrast", 100}, {"saturation", 100}}
    });

    reg.registerEffect({
        "glow", "発光", "",
        {{"strength", 50}, {"radius", 10}, {"color", "#ffffff"}}
    });
    
    // 将来的にはここでJSONやLuaから動的にロードすることも可能
}

}

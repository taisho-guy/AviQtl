// ui/qml/common/shaders/blend.glsl
#ifndef RINA_BLEND_GLSL
#define RINA_BLEND_GLSL

// 各ブレンド関数は、base（背景色）と blend（前景色）を受け取り、合成結果を返します。
// アルファ合成は別途行うことを想定しています。

vec3 blendNormal(vec3 base, vec3 blend) {
    return blend;
}

vec3 blendMultiply(vec3 base, vec3 blend) {
    return base * blend;
}

vec3 blendScreen(vec3 base, vec3 blend) {
    return 1.0 - (1.0 - base) * (1.0 - blend);
}

vec3 blendOverlay(vec3 base, vec3 blend) {
    return mix(2.0 * base * blend, 1.0 - 2.0 * (1.0 - base) * (1.0 - blend), step(0.5, base));
}

vec3 blendDarken(vec3 base, vec3 blend) {
    return min(base, blend);
}

vec3 blendLighten(vec3 base, vec3 blend) {
    return max(base, blend);
}

vec3 blendAdd(vec3 base, vec3 blend) {
    return min(base + blend, vec3(1.0));
}

vec3 blendDifference(vec3 base, vec3 blend) {
    return abs(base - blend);
}

#endif

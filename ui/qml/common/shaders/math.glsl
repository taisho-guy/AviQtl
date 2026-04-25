// ui/qml/common/shaders/math.glsl
#ifndef AVIQTL_MATH_GLSL
#define AVIQTL_MATH_GLSL

const float PI = 3.14159265359;
const float TWO_PI = 6.28318530718;
const float HALF_PI = 1.57079632679;

// 値の範囲変換 (map / remap)
// valueを low1-high1 の範囲から low2-high2 の範囲へ変換
float remap(float value, float low1, float high1, float low2, float high2) {
    return low2 + (value - low1) * (high2 - low2) / (high1 - low1);
}

// ステップ関数（エッジを滑らかにする）
float smoothStepInOut(float low, float high, float value) {
    float t = clamp((value - low) / (high - low), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// 輝度計算 (Rec.709)
float luminance(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

// 2D回転行列
mat2 rotate2d(float angle) {
    return mat2(cos(angle), -sin(angle),
                sin(angle),  cos(angle));
}

// UV座標を中心基準にスケーリング
vec2 scaleUV(vec2 uv, vec2 scale, vec2 center) {
    return (uv - center) / scale + center;
}

#endif

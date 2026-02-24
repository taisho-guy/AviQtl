#version 440
#extension GL_GOOGLE_include_directive : enable
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float brightness;
    float contrast;
    float hue;
    float saturation;
    float lightness;
};
layout(binding = 1) uniform sampler2D source;

#include "../common/shaders/utils.glsl"

void main() {
    vec4 tex = texture(source, qt_TexCoord0);
    vec3 rgb = tex.rgb;
    
    // 1. 明るさ (Brightness) - 加算
    rgb = rgb + brightness;
    
    // 2. コントラスト (Contrast)
    rgb = adjustContrast(rgb, contrast + 1.0);
    
    // 3. HSV変換
    vec3 hsv = rgb2hsv(rgb);
    
    // 色相 (Hue) - 回転
    hsv.x += hue;
    if (hsv.x < 0.0) hsv.x += 1.0;
    if (hsv.x > 1.0) hsv.x -= 1.0;
    
    // 彩度 (Saturation) - 乗算
    hsv.y *= (saturation + 1.0);
    
    // 輝度 (Lightness/Value) - 乗算
    hsv.z *= (lightness + 1.0);
    
    rgb = hsv2rgb(hsv);
    
    fragColor = vec4(rgb, tex.a) * qt_Opacity;
}
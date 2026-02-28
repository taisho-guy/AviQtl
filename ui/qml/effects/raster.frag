#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float amplitude;
    float frequency;
    float time;
    float vertical;
    float targetWidth;
    float targetHeight;
};

void main() {
    vec2 tc = qt_TexCoord0;
    
    float offset = 0.0;
    
    if (vertical > 0.5) {
        // 縦波 (X座標に応じてYをずらす)
        offset = sin(tc.x * frequency + time) * (amplitude / targetHeight);
        tc.y += offset;
    } else {
        // 横波 (Y座標に応じてXをずらす)
        offset = sin(tc.y * frequency + time) * (amplitude / targetWidth);
        tc.x += offset;
    }
    
    // 範囲外は透明にする (または clamp して引き伸ばす)
    if (tc.x < 0.0 || tc.x > 1.0 || tc.y < 0.0 || tc.y > 1.0) {
        fragColor = vec4(0.0);
    } else {
        fragColor = texture(source, tc) * qt_Opacity;
    }
}
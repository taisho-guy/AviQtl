#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float horizontal;
    float vertical;
    float centerX;
    float centerY;
    float invertLuma;
    float invertChroma;
    float targetWidth;
    float targetHeight;
};

void main() {
    vec2 tc = qt_TexCoord0;
    
    // 中心点のオフセット (正規化座標)
    vec2 centerOffset = vec2(centerX / targetWidth, centerY / targetHeight);
    vec2 center = vec2(0.5) + centerOffset;
    
    // 座標反転
    if (horizontal > 0.5) {
        tc.x = center.x + (center.x - tc.x);
    }
    if (vertical > 0.5) {
        tc.y = center.y + (center.y - tc.y);
    }
    
    // 範囲外チェック (ミラーリングで範囲外を参照しないようにclampするか、透明にするか)
    // ここでは透明にする
    if (tc.x < 0.0 || tc.x > 1.0 || tc.y < 0.0 || tc.y > 1.0) {
        fragColor = vec4(0.0);
        return;
    }
    
    vec4 c = texture(source, tc);
    
    // 輝度・色相反転 (簡易実装: RGB反転)
    if (invertLuma > 0.5 || invertChroma > 0.5) {
        c.rgb = 1.0 - c.rgb;
    }
    
    fragColor = c * qt_Opacity;
}
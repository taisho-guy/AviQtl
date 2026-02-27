#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 direction;
    float radius;
    float targetWidth;
    float targetHeight;
};

void main() {
    vec2 tc = qt_TexCoord0;
    vec4 c = vec4(0.0);
    float totalWeight = 0.0;
    
    // ピクセル単位の移動量に変換
    vec2 texel = vec2(1.0 / max(1.0, targetWidth), 1.0 / max(1.0, targetHeight));
    vec2 offset = direction * texel;
    
    if (radius <= 0.0) {
        fragColor = texture(source, tc) * qt_Opacity;
        return;
    }

    int steps = int(clamp(radius, 1.0, 32.0));
    float stepSize = radius / float(max(1, steps));
    
    for (int i = -steps; i <= steps; ++i) {
        float fi = float(i);
        vec2 sampleTc = tc + offset * fi * stepSize;
        
        // 画面外のサンプリングを防ぐ (Clamp To Edge)
        sampleTc = clamp(sampleTc, 0.0, 1.0);
        
        // ガウス関数近似による重み付け
        float weight = exp(-(fi * fi) / (float(steps * steps) * 0.5 + 0.1));
        c += texture(source, sampleTc) * weight;
        totalWeight += weight;
    }
    
    if (totalWeight > 0.0) {
        c /= totalWeight;
    }
    
    fragColor = c * qt_Opacity;
}

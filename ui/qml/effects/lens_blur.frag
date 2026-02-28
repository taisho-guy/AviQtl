#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float radius;
    float brightness;
    float targetWidth;
    float targetHeight;
};

void main() {
    vec2 tc = qt_TexCoord0;
    
    if (radius <= 0.0) {
        fragColor = texture(source, tc) * qt_Opacity;
        return;
    }

    vec2 texel = vec2(1.0 / max(1.0, targetWidth), 1.0 / max(1.0, targetHeight));
    vec4 acc = vec4(0.0);
    float totalWeight = 0.0;
    
    // 黄金角を用いたサンプリングで円形ボケを近似
    float goldenAngle = 2.39996323;
    float sampleCount = min(64.0, max(16.0, radius * 2.0));
    
    for (float i = 0.0; i < sampleCount; i++) {
        float r = sqrt((i + 0.5) / sampleCount) * radius;
        float theta = i * goldenAngle;
        
        vec2 offset = vec2(cos(theta), sin(theta)) * r * texel;
        vec4 col = texture(source, tc + offset);
        
        // 輝度が高いピクセルの重みを上げてボケのハイライトを強調
        float lum = dot(col.rgb, vec3(0.299, 0.587, 0.114));
        float weight = 1.0 + (pow(lum, 4.0) * brightness * 0.1);
        
        acc += col * weight;
        totalWeight += weight;
    }
    
    fragColor = (acc / totalWeight) * qt_Opacity;
}
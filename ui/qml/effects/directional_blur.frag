#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float angle;
    float length;
    float targetWidth;
    float targetHeight;
};

void main() {
    vec2 tc = qt_TexCoord0;
    
    if (length <= 0.0) {
        fragColor = texture(source, tc) * qt_Opacity;
        return;
    }

    float rad = radians(angle);
    vec2 dir = vec2(cos(rad), sin(rad));
    vec2 texel = vec2(1.0 / max(1.0, targetWidth), 1.0 / max(1.0, targetHeight));
    
    vec4 color = vec4(0.0);
    float totalWeight = 0.0;
    
    int steps = int(clamp(length, 1.0, 32.0));
    for (int i = -steps; i <= steps; ++i) {
        float t = float(i) / float(steps);
        vec2 offset = dir * texel * (length * 0.5) * t;
        color += texture(source, tc + offset);
        totalWeight += 1.0;
    }
    
    fragColor = (color / totalWeight) * qt_Opacity;
}
#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float samples;
    float strength;
    float centerX;
    float centerY;
    float targetWidth;
    float targetHeight;
};

void main() {
    vec2 tc = qt_TexCoord0;
    vec2 center = vec2(0.5 + centerX / targetWidth, 0.5 + centerY / targetHeight);
    vec2 dir = tc - center;
    float dist = length(dir);
    dir /= dist;

    vec4 color = vec4(0.0);
    float totalWeight = 0.0;

    int nSamples = int(clamp(samples, 1.0, 32.0));
    for (int i = 0; i < nSamples; i++) {
        float scale = 1.0 - strength * (float(i) / float(nSamples - 1));
        vec4 sampleColor = texture(source, center + (tc - center) * scale);
        color += sampleColor;
        totalWeight += 1.0;
    }

    fragColor = (color / totalWeight) * qt_Opacity;
}
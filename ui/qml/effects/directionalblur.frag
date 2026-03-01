#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float angle;
    float len;
    float targetWidth;
    float targetHeight;
};
layout(binding=1) uniform sampler2D source;
void main() {
    float rad  = radians(angle);
    vec2  dir  = vec2(cos(rad), -sin(rad));
    vec2  texel = vec2(1.0 / targetWidth, 1.0 / targetHeight);
    vec2  step  = dir * texel * len;
    const int   SAMPLES = 16;
    vec4  sum   = vec4(0.0);
    float total = 0.0;
    for (int i = 0; i < SAMPLES; i++) {
        float t = float(i) / float(SAMPLES - 1) - 0.5;
        float w = 1.0 - abs(t) * 1.6;
        w = max(w, 0.0);
        sum   += texture(source, qt_TexCoord0 + step * t * 2.0) * w;
        total += w;
    }
    fragColor = (sum / max(total, 0.0001)) * qt_Opacity;
}

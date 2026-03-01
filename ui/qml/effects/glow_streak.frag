#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float diffusion;
    float blurSoft;
    float shape;
    float targetWidth;
    float targetHeight;
};
layout(binding=1) uniform sampler2D source;
const int SAMPLES = 20;
vec4 sampleDir(sampler2D s, vec2 uv, vec2 dir, float r, float tw, float th) {
    vec2  step  = dir * vec2(r / tw, r / th);
    vec4  sum   = vec4(0.0);
    float total = 0.0;
    for (int i = -SAMPLES; i <= SAMPLES; i++) {
        float t = float(i) / float(SAMPLES);
        float w = exp(-t * t * (4.0 + blurSoft * 0.1));
        sum    += texture(s, uv + step * t) * w;
        total  += w;
    }
    return sum / max(total, 0.001);
}
void main() {
    float r   = diffusion;
    float asp = targetWidth / max(targetHeight, 1.0);
    vec4  result = vec4(0.0);
    if (shape < 0.5) {
        vec4 h = sampleDir(source, qt_TexCoord0, vec2(1.0, 0.0), r, targetWidth, targetHeight);
        vec4 v = sampleDir(source, qt_TexCoord0, vec2(0.0, 1.0), r, targetWidth, targetHeight);
        result = (h + v) * 0.5;
    } else if (shape < 1.5) {
        vec4 h = sampleDir(source, qt_TexCoord0, vec2(1.0, 0.0), r, targetWidth, targetHeight);
        vec4 v = sampleDir(source, qt_TexCoord0, vec2(0.0, 1.0), r, targetWidth, targetHeight);
        result = (h + v) * 0.5;
    } else if (shape < 2.5) {
        float s2 = 0.7071;
        vec4 d1 = sampleDir(source, qt_TexCoord0, vec2(s2,  s2), r, targetWidth, targetHeight);
        vec4 d2 = sampleDir(source, qt_TexCoord0, vec2(s2, -s2), r, targetWidth, targetHeight);
        result = (d1 + d2) * 0.5;
    } else if (shape < 3.5) {
        float s2 = 0.7071;
        vec4 h  = sampleDir(source, qt_TexCoord0, vec2(1.0, 0.0), r, targetWidth, targetHeight);
        vec4 v  = sampleDir(source, qt_TexCoord0, vec2(0.0, 1.0), r, targetWidth, targetHeight);
        vec4 d1 = sampleDir(source, qt_TexCoord0, vec2(s2,  s2),  r, targetWidth, targetHeight);
        vec4 d2 = sampleDir(source, qt_TexCoord0, vec2(s2, -s2),  r, targetWidth, targetHeight);
        result = (h + v + d1 + d2) * 0.25;
    } else if (shape < 4.5) {
        result = sampleDir(source, qt_TexCoord0, vec2(1.0, 0.0), r, targetWidth, targetHeight);
    } else {
        result = sampleDir(source, qt_TexCoord0, vec2(0.0, 1.0), r, targetWidth, targetHeight);
    }
    fragColor = result * qt_Opacity;
}

#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float radius;
    float brightness;
    float targetWidth;
    float targetHeight;
};
layout(binding=1) uniform sampler2D source;
const float GOLDEN_ANGLE = 2.399963229728;
const int   MAX_SAMPLES  = 64;
void main() {
    if (radius < 0.5) {
        fragColor = texture(source, qt_TexCoord0) * qt_Opacity;
        return;
    }
    vec2  texel = vec2(radius / targetWidth, radius / targetHeight);
    vec4  sum   = vec4(0.0);
    int   n     = MAX_SAMPLES;
    for (int i = 0; i < n; i++) {
        float r     = sqrt(float(i + 1) / float(n + 1));
        float theta = float(i) * GOLDEN_ANGLE;
        vec2  off   = vec2(cos(theta), sin(theta)) * r * texel;
        vec4  s     = texture(source, qt_TexCoord0 + off);
        float luma  = dot(s.rgb, vec3(0.299, 0.587, 0.114));
        float boost = 1.0 + brightness * luma * 0.05;
        sum += s * boost;
    }
    fragColor = (sum / float(n)) * qt_Opacity;
}

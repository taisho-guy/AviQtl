#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float edgeSize;
    float blur;
    float _p0;
    vec4  edgeColor;
    float targetWidth;
    float targetHeight;
};
layout(binding=1) uniform sampler2D source;
const float PI2 = 6.28318530717959;
void main() {
    vec2  texel   = vec2(1.0 / targetWidth, 1.0 / targetHeight);
    vec4  col     = texture(source, qt_TexCoord0);
    float maxAlpha = 0.0;
    const int N   = 24;
    for (int i = 0; i < N; i++) {
        float theta = float(i) * PI2 / float(N);
        vec2  off   = vec2(cos(theta), sin(theta)) * edgeSize * texel;
        maxAlpha    = max(maxAlpha, texture(source, qt_TexCoord0 + off).a);
    }
    float soft    = max(blur * 0.01, 0.001);
    float edgeMask = smoothstep(col.a - soft, col.a + soft, maxAlpha) * (1.0 - col.a);
    vec4  outline = vec4(edgeColor.rgb, edgeColor.a * edgeMask);
    float outA    = outline.a + col.a * (1.0 - outline.a);
    vec3  outRGB  = (outline.rgb * outline.a + col.rgb * col.a * (1.0 - outline.a))
                    / max(outA, 0.0001);
    fragColor     = vec4(outRGB, outA) * qt_Opacity;
}

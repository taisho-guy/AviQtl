#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float strength;
    float flashX;
    float flashY;
    float targetWidth;
    float targetHeight;
};
layout(binding=1) uniform sampler2D source;
void main() {
    vec2  flash   = vec2(0.5 + flashX / targetWidth,
                         0.5 - flashY / targetHeight);
    vec2  dir     = flash - qt_TexCoord0;
    float scale   = strength * 0.004;
    const int N   = 24;
    vec4  sum     = vec4(0.0);
    float total   = 0.0;
    for (int i = 0; i < N; i++) {
        float t   = float(i) / float(N - 1);
        float w   = 1.0 - t * 0.8;
        vec2  uv  = qt_TexCoord0 + dir * t * scale;
        sum      += texture(source, clamp(uv, vec2(0.0), vec2(1.0))) * w;
        total    += w;
    }
    fragColor = (sum / max(total, 0.001)) * qt_Opacity;
}

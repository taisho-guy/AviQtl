#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float samples;
    float strength;
    float centerX;
    float centerY;
    float targetWidth;
    float targetHeight;
};
layout(binding=1) uniform sampler2D source;
void main() {
    vec2  center = vec2(0.5 + centerX / targetWidth,
                        0.5 - centerY / targetHeight);
    vec2  dir    = qt_TexCoord0 - center;
    int   n      = max(1, int(samples));
    float step   = strength / float(n) * 0.002;
    vec4  sum    = vec4(0.0);
    for (int i = 0; i < n; i++) {
        float t   = float(i) / float(max(n - 1, 1));
        vec2  uv  = qt_TexCoord0 - dir * t * step * float(n);
        sum += texture(source, clamp(uv, vec2(0.0), vec2(1.0)));
    }
    fragColor = (sum / float(n)) * qt_Opacity;
}

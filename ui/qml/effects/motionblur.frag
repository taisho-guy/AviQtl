#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float quality;
    float shutterSpeed;
    float velX;
    float velY;
    float trail;
    float targetWidth;
    float targetHeight;
};
layout(binding=1) uniform sampler2D source;
void main() {
    int   n       = max(1, int(quality));
    vec2  vel     = vec2(velX / targetWidth, -velY / targetHeight) * shutterSpeed * 0.01;
    float lenSq   = dot(vel, vel);
    if (lenSq < 0.000001) {
        fragColor = texture(source, qt_TexCoord0) * qt_Opacity;
        return;
    }
    vec4  sum   = vec4(0.0);
    float total = 0.0;
    for (int i = 0; i < n; i++) {
        float t   = float(i) / float(max(n - 1, 1));
        float w   = trail > 0.5 ? 1.0 - t * 0.7 : 1.0;
        vec2  uv  = qt_TexCoord0 - vel * t;
        sum      += texture(source, clamp(uv, vec2(0.0), vec2(1.0))) * w;
        total    += w;
    }
    fragColor = (sum / max(total, 0.0001)) * qt_Opacity;
}

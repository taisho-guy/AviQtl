#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2  direction;
    float radius;
    float gain;
    float targetWidth;
    float targetHeight;
};
layout(binding=1) uniform sampler2D source;
void main() {
    vec2 step = vec2(direction.x / targetWidth, direction.y / targetHeight);
    int  r    = int(clamp(radius, 0.0, 256.0));
    if (r == 0) {
        fragColor = texture(source, qt_TexCoord0) * qt_Opacity;
        return;
    }
    vec4  sum   = vec4(0.0);
    float total = 0.0;
    for (int i = -r; i <= r; i++) {
        float w = exp(-0.5 * float(i * i) / float(max(r, 1) * max(r, 1)) * 9.0);
        sum   += texture(source, qt_TexCoord0 + step * float(i)) * w;
        total += w;
    }
    vec4 result = sum / max(total, 0.0001);
    result.rgb += result.rgb * gain;
    fragColor   = result * qt_Opacity;
}

#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float centerX;
    float centerY;
    float rippleWidth;
    float rippleHeight;
    float speed;
    float time;
    float rippleCount;
    float rippleInterval;
    float amplitudeDecay;
    float targetWidth;
    float targetHeight;
};
layout(binding=1) uniform sampler2D source;
const float PI = 3.14159265;
void main() {
    vec2  center = vec2(0.5 + centerX / targetWidth,
                        0.5 - centerY / targetHeight);
    vec2  d      = (qt_TexCoord0 - center) * vec2(targetWidth, targetHeight);
    float dist   = length(d);
    float freq   = (rippleWidth > 0.001) ? (2.0 * PI / rippleWidth) : 0.0;
    float phase  = dist * freq - time * speed * 0.01;
    float ripple = sin(phase);
    float amp    = rippleHeight / max(targetHeight, 1.0);
    if (rippleCount > 0.5) {
        float ringIdx = (dist - time * speed * 0.01 / freq * freq) / rippleWidth;
        float inRing  = step(0.0, ringIdx) * step(ringIdx, rippleCount * rippleInterval);
        amp *= inRing;
    }
    if (abs(amplitudeDecay) > 0.001) {
        float ringNum  = dist / rippleWidth;
        float peakRing = abs(amplitudeDecay);
        float decay    = (amplitudeDecay >= 0.0)
                         ? clamp(ringNum / peakRing, 0.0, 1.0)
                         : clamp(1.0 - ringNum / peakRing, 0.0, 1.0);
        amp *= decay;
    }
    vec2 dir = (dist > 0.001) ? (d / dist) : vec2(0.0);
    vec2 uv  = qt_TexCoord0 + dir * ripple * amp;
    fragColor = texture(source, clamp(uv, vec2(0.0), vec2(1.0))) * qt_Opacity;
}

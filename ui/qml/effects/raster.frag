#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float amplitude;
    float frequency;
    float period;
    float time;
    float vertical;
    float randomAmp;
    float targetWidth;
    float targetHeight;
};
layout(binding=1) uniform sampler2D source;
float hash(float n) { return fract(sin(n) * 43758.5453); }
void main() {
    vec2  uv   = qt_TexCoord0;
    float freq = (frequency > 0.001) ? (2.0 * 3.14159265 * targetHeight / frequency) : 0.0;
    if (vertical < 0.5) {
        float row  = floor(uv.y * targetHeight);
        float rnd  = (randomAmp > 0.0) ? (1.0 + (hash(row) * 2.0 - 1.0) * randomAmp) : 1.0;
        float wave = sin(uv.y * freq + time * period * 2.0 * 3.14159265) * rnd;
        uv.x += wave * amplitude / targetWidth;
    } else {
        float col  = floor(uv.x * targetWidth);
        float rnd  = (randomAmp > 0.0) ? (1.0 + (hash(col) * 2.0 - 1.0) * randomAmp) : 1.0;
        float wave = sin(uv.x * freq + time * period * 2.0 * 3.14159265) * rnd;
        uv.y += wave * amplitude / targetHeight;
    }
    fragColor = texture(source, clamp(uv, vec2(0.0), vec2(1.0))) * qt_Opacity;
}

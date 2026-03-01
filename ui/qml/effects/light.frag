#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float strength;
    float diffusion;
    float ratio;
    float backlight;
    float targetWidth;
    float targetHeight;
};
layout(binding=1) uniform sampler2D source;
void main() {
    vec4  col  = texture(source, qt_TexCoord0);
    float asp  = targetWidth / max(targetHeight, 1.0);
    vec2  d    = qt_TexCoord0 - 0.5;
    float dist = length(d * vec2(asp, 1.0)) / max(diffusion / 100.0 * 0.8, 0.001);
    float r    = clamp(ratio / 100.0, -1.0, 1.0);
    float innerEdge = max(0.0, r);
    float lightMask;
    if (r >= 0.0) {
        lightMask = 1.0 - smoothstep(innerEdge, 1.2, dist);
    } else {
        lightMask = smoothstep(0.0, 1.0 + r, dist);
    }
    if (backlight > 0.5) lightMask = 1.0 - lightMask;
    float s    = strength / 100.0;
    vec3  rgb  = clamp(col.rgb + col.rgb * lightMask * s, 0.0, 1.0);
    fragColor  = vec4(rgb, col.a) * qt_Opacity;
}

#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float strength;
};
layout(binding=1) uniform sampler2D source;
layout(binding=2) uniform sampler2D glowSource;
void main() {
    vec4 col  = texture(source,    qt_TexCoord0);
    vec4 glow = texture(glowSource, qt_TexCoord0);
    vec3 rgb  = clamp(col.rgb + glow.rgb * strength, 0.0, 1.0);
    fragColor = vec4(rgb, col.a) * qt_Opacity;
}

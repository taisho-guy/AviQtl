#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float strength;
    float glowOnly;
};
layout(binding=1) uniform sampler2D source;
layout(binding=2) uniform sampler2D glowSource;
void main() {
    vec4 col  = texture(source,     qt_TexCoord0);
    vec4 glow = texture(glowSource, qt_TexCoord0);
    vec4 result;
    if (glowOnly > 0.5) {
        result = vec4(glow.rgb * strength, glow.a);
    } else {
        result = vec4(clamp(col.rgb + glow.rgb * strength, 0.0, 1.0), col.a);
    }
    fragColor = result * qt_Opacity;
}

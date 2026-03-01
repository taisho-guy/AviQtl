#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float shadowOnly;
};
layout(binding=1) uniform sampler2D source;
layout(binding=2) uniform sampler2D shadowSource;
void main() {
    vec4 col    = texture(source,       qt_TexCoord0);
    vec4 shadow = texture(shadowSource, qt_TexCoord0);
    vec4 result;
    if (shadowOnly > 0.5) {
        result = shadow;
    } else {
        float a   = shadow.a + col.a * (1.0 - shadow.a);
        vec3  rgb = (shadow.rgb * shadow.a + col.rgb * col.a * (1.0 - shadow.a))
                    / max(a, 0.001);
        result    = vec4(rgb, a);
    }
    fragColor = result * qt_Opacity;
}

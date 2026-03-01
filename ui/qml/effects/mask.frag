#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
};
layout(binding=1) uniform sampler2D source;
layout(binding=2) uniform sampler2D maskSource;
void main() {
    vec4 blurred  = texture(source,     qt_TexCoord0);
    vec4 original = texture(maskSource, qt_TexCoord0);
    fragColor = vec4(blurred.rgb, blurred.a * original.a) * qt_Opacity;
}

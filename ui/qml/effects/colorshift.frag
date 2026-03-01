#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float shiftWidth;
    float angle;
    float strength;
    float targetWidth;
    float targetHeight;
};
layout(binding=1) uniform sampler2D source;
void main() {
    float rad  = radians(angle);
    vec2  dir  = vec2(cos(rad), -sin(rad));
    vec2  off  = dir * (shiftWidth * strength / 100.0)
                     * vec2(1.0 / targetWidth, 1.0 / targetHeight);
    float r = texture(source, qt_TexCoord0 + off).r;
    float g = texture(source, qt_TexCoord0).g;
    float b = texture(source, qt_TexCoord0 - off).b;
    float a = texture(source, qt_TexCoord0).a;
    fragColor = vec4(r, g, b, a) * qt_Opacity;
}

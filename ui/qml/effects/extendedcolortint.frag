#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float rScale;
    float gScale;
    float bScale;
};
layout(binding=1) uniform sampler2D source;
void main() {
    vec4 col  = texture(source, qt_TexCoord0);
    vec3 rgb  = vec3(col.r * rScale,
                     col.g * gScale,
                     col.b * bScale);
    fragColor = vec4(clamp(rgb, 0.0, 1.0), col.a) * qt_Opacity;
}

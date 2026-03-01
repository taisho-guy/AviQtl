#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float strength;
    float _p0; float _p1; float _p2;
    vec4  monoColor;
};
layout(binding=1) uniform sampler2D source;
void main() {
    vec4  col  = texture(source, qt_TexCoord0);
    float luma = dot(col.rgb, vec3(0.299, 0.587, 0.114));
    vec3  mono = monoColor.rgb * luma;
    fragColor  = vec4(mix(col.rgb, mono, strength), col.a) * qt_Opacity;
}

#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float threshold;
    float _p0; float _p1;
    vec4  lightColor;
};
layout(binding=1) uniform sampler2D source;
void main() {
    vec4  col    = texture(source, qt_TexCoord0);
    float luma   = dot(col.rgb, vec3(0.299, 0.587, 0.114));
    float factor = clamp(luma - threshold, 0.0, 1.0);
    vec3  tinted = (lightColor.a > 0.001)
                   ? col.rgb * lightColor.rgb * factor
                     / max(dot(lightColor.rgb, vec3(0.299,0.587,0.114)), 0.001)
                   : col.rgb * factor;
    fragColor = vec4(tinted, col.a * factor) * qt_Opacity;
}

#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float compositeMode;  // 0=上合成 1=下合成 2=光のみ
};
layout(binding=1) uniform sampler2D source;
layout(binding=2) uniform sampler2D flashSource;
void main() {
    vec4 col   = texture(source,      qt_TexCoord0);
    vec4 flash = texture(flashSource, qt_TexCoord0);
    vec4 result;
    if (compositeMode < 0.5) {
        vec3 rgb = clamp(col.rgb + flash.rgb, 0.0, 1.0);
        result   = vec4(rgb, col.a);
    } else if (compositeMode < 1.5) {
        vec3 rgb = clamp(flash.rgb + col.rgb, 0.0, 1.0);
        result   = vec4(rgb, flash.a);
    } else {
        result   = flash;
    }
    fragColor = result * qt_Opacity;
}

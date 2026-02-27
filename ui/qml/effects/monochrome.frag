#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 monoColor;
    float strength;
};

void main() {
    vec4 c = texture(source, qt_TexCoord0);
    if (c.a == 0.0) {
        fragColor = vec4(0.0);
        return;
    }

    vec3 unmultiplied = c.rgb / c.a;
    float luminance = dot(unmultiplied, vec3(0.299, 0.587, 0.114));

    // 指定色と輝度を乗算
    vec3 colored = monoColor.rgb * luminance;

    // 強さに応じて元の色とブレンドし、再度アルファ乗算
    c.rgb = mix(unmultiplied, colored, strength) * c.a;

    fragColor = c * qt_Opacity;
}

#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D source;
layout(binding = 2) uniform sampler2D maskSource;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};

void main() {
    vec4 c = texture(source, qt_TexCoord0);
    vec4 m = texture(maskSource, qt_TexCoord0);

    if (m.a <= 0.0) {
        fragColor = vec4(0.0);
    } else {
        fragColor = c * qt_Opacity;
    }
}

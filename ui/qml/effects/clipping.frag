#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float topVal;
    float bottomVal;
    float leftVal;
    float rightVal;
    float targetWidth;
    float targetHeight;
};

void main() {
    vec2 tc = qt_TexCoord0;
    float px = tc.x * targetWidth;
    float py = tc.y * targetHeight;

    if (px < leftVal || px > (targetWidth - rightVal) || 
        py < topVal || py > (targetHeight - bottomVal)) {
        fragColor = vec4(0.0);
    } else {
        fragColor = texture(source, tc) * qt_Opacity;
    }
}

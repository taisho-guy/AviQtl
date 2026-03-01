#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float offsetX;
    float offsetY;
    float density;
    float targetWidth;
    float targetHeight;
};
layout(binding=1) uniform sampler2D source;
void main() {
    vec2  off    = vec2(offsetX / targetWidth, -offsetY / targetHeight);
    vec4  shadow = texture(source, qt_TexCoord0 - off);
    float alpha  = shadow.a * density;
    fragColor    = vec4(0.0, 0.0, 0.0, alpha) * qt_Opacity;
}

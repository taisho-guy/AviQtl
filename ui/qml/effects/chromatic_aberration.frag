#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 redOffset;
    vec2 greenOffset;
    vec2 blueOffset;
    float targetWidth;
    float targetHeight;
};

void main() {
    vec2 tc = qt_TexCoord0;
    vec2 texel = vec2(1.0 / max(1.0, targetWidth), 1.0 / max(1.0, targetHeight));
    
    vec2 rTc = tc + redOffset * texel;
    vec2 gTc = tc + greenOffset * texel;
    vec2 bTc = tc + blueOffset * texel;
    
    float r = texture(source, rTc).r;
    float g = texture(source, gTc).g;
    float b = texture(source, bTc).b;
    float a = (texture(source, rTc).a + texture(source, gTc).a + texture(source, bTc).a) / 3.0;
    
    fragColor = vec4(r, g, b, a) * qt_Opacity;
}
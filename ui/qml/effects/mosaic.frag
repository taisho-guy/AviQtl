#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float size;
    float targetWidth;
    float targetHeight;
};
layout(binding = 1) uniform sampler2D source;

void main() {
    vec2 uv = qt_TexCoord0;
    
    if (size > 1.0 && targetWidth > 0.0 && targetHeight > 0.0) {
        vec2 res = vec2(targetWidth, targetHeight);
        vec2 blocks = res / size;
        uv = floor(uv * blocks) / blocks;
        // ブロックの中心をサンプリング
        uv += (vec2(1.0) / blocks) * 0.5;
    }
    
    fragColor = texture(source, uv) * qt_Opacity;
}

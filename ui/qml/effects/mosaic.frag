#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float size;
    float targetWidth;
    float targetHeight;
};

void main() {
    vec2 tc = qt_TexCoord0;
    
    if (size <= 1.0) {
        fragColor = texture(source, tc) * qt_Opacity;
        return;
    }
    
    // ピクセル座標に変換
    float px = tc.x * targetWidth;
    float py = tc.y * targetHeight;
    
    // 指定サイズで量子化（モザイクのブロック中心座標を計算）
    float mx = floor(px / size) * size + (size * 0.5);
    float my = floor(py / size) * size + (size * 0.5);
    
    vec2 mosaicTc = vec2(mx / targetWidth, my / targetHeight);
    
    fragColor = texture(source, mosaicTc) * qt_Opacity;
}

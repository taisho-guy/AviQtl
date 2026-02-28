#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float xStrength;
    float yStrength;
    float time;
    float targetWidth;
    float targetHeight;
};

// 簡易的な乱数生成
float rand(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec2 tc = qt_TexCoord0;
    
    // 時間に基づいてランダムなオフセットを生成
    // floor(time) を使うことで、フレームごとに不連続な動きにする
    float t = floor(time);
    float offsetX = (rand(vec2(t, 0.0)) - 0.5) * 2.0 * (xStrength / targetWidth);
    float offsetY = (rand(vec2(0.0, t)) - 0.5) * 2.0 * (yStrength / targetHeight);
    
    vec2 newTc = tc + vec2(offsetX, offsetY);
    
    fragColor = texture(source, newTc) * qt_Opacity;
}
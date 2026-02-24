#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 direction;
    float radius;
    float width;
    float height;
};
layout(binding = 1) uniform sampler2D source;

// ガウス関数
float gaussian(float x, float sigma) {
    return exp(-(x*x) / (2.0 * sigma * sigma));
}

void main() {
    if (radius <= 0.0) {
        fragColor = texture(source, qt_TexCoord0) * qt_Opacity;
        return;
    }

    vec2 res = vec2(width, height);
    vec2 step = direction / res;
    
    vec4 acc = vec4(0.0);
    float weightSum = 0.0;
    
    // シグマの近似
    float sigma = max(radius / 2.0, 0.1);
    
    // ループ回数の制限 (GPU負荷対策)
    // 半径が大きい場合はサンプリング数を制限するが、品質維持のため最大64サンプルとする
    int samples = int(min(ceil(radius), 64.0));
    
    for (int i = -samples; i <= samples; i++) {
        float x = float(i);
        float w = gaussian(x, sigma);
        vec2 uv = qt_TexCoord0 + x * step;
        acc += texture(source, uv) * w;
        weightSum += w;
    }
    
    fragColor = (acc / weightSum) * qt_Opacity;
}

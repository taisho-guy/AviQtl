#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float quality;
    float shutterSpeed;
    float prevFrame;
    float currentFrame;
    float targetWidth;
    float targetHeight;
};

void main() {
    vec2 tc = qt_TexCoord0;

    // このシェーダーはオブジェクトの transform を直接参照できないため、
    // QML側で前フレームと現フレームのオブジェクト位置を計算し、
    // その差分を velocity として渡すのが理想。
    // ここでは簡易的に、UV座標の差としてモーションを近似する。
    // (正しく実装するには uniform で prevMvpMatrix と currentMvpMatrix を受け取る必要がある)
    vec2 velocity = (qt_Matrix * vec4(tc, 0.0, 1.0)).xy - (vec4(tc, 0.0, 1.0)).xy;
    velocity *= shutterSpeed * 0.01;

    if (length(velocity) < 0.0001) {
        fragColor = texture(source, tc) * qt_Opacity;
        return;
    }

    vec4 color = vec4(0.0);
    int nSamples = int(clamp(quality, 1.0, 64.0));

    for (int i = 0; i < nSamples; i++) {
        float t = float(i) / float(max(1, nSamples - 1));
        color += texture(source, tc - velocity * t);
    }

    fragColor = (color / float(nSamples)) * qt_Opacity;
}
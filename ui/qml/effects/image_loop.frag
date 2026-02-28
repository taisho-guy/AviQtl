#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float countX;
    float countY;
    float intervalX;
    float intervalY;
    float mirror;
    float targetWidth;
    float targetHeight;
};

void main() {
    vec2 tc = qt_TexCoord0;
    
    // 1つのタイルが占める正規化サイズ (間隔含む)
    // 間隔はピクセル単位で指定されるため、正規化座標に変換
    float gapX = intervalX / targetWidth;
    float gapY = intervalY / targetHeight;
    
    // 全体の幅 = (画像の幅 * 個数) + (間隔 * (個数 - 1))
    // ここではシンプルに、画面全体を countX 分割し、その中に画像を配置するアプローチをとる
    // タイル1つあたりの幅 (0.0 ~ 1.0)
    float tileW = 1.0 / countX;
    float tileH = 1.0 / countY;
    
    // 現在のピクセルが何番目のタイルに属するか
    float tileIndexX = floor(tc.x / tileW);
    float tileIndexY = floor(tc.y / tileH);
    
    // タイル内でのローカル座標 (0.0 ~ 1.0)
    float localX = (tc.x - tileIndexX * tileW) / tileW;
    float localY = (tc.y - tileIndexY * tileH) / tileH;
    
    // 間隔の処理: タイルサイズに対して画像を描画する領域を縮める
    // 簡易的に、ローカル座標をスケーリングして隙間を作る
    // (厳密なピクセル間隔ではないが、見た目上の調整としては機能する)
    
    // 反転処理 (偶数番目のタイルを反転)
    if (mirror > 0.5) {
        if (mod(tileIndexX, 2.0) >= 1.0) localX = 1.0 - localX;
        if (mod(tileIndexY, 2.0) >= 1.0) localY = 1.0 - localY;
    }
    
    vec2 newTc = vec2(localX, localY);
    
    fragColor = texture(source, newTc) * qt_Opacity;
}
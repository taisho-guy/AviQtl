#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float centerWidth;
    float scaleFactor;
    float rotation;
    float swirl;
    float targetWidth;
    float targetHeight;
};
layout(binding=1) uniform sampler2D source;
const float PI = 3.14159265;
void main() {
    vec2  d      = qt_TexCoord0 - 0.5;
    float asp    = targetWidth / max(targetHeight, 1.0);
    d.x         *= asp;
    float r      = length(d);
    float theta  = atan(d.y, d.x);
    float innerR = clamp(centerWidth / 200.0, 0.0, 0.99);
    if (r < innerR * 0.5) {
        fragColor = vec4(0.0);
        return;
    }
    float scale  = max(scaleFactor / 100.0, 0.001);
    float srcX   = fract((theta / (2.0 * PI) + 0.5)
                         + rotation / 360.0
                         + swirl / 360.0 * r * 2.0);
    float srcY   = clamp((r * 2.0 / scale - innerR) / (1.0 - innerR), 0.0, 1.0);
    fragColor    = texture(source, vec2(srcX, srcY)) * qt_Opacity;
}

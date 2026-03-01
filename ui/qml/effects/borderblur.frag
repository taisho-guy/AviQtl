#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float size;
    float blurAlpha;
    float targetWidth;
    float targetHeight;
};
layout(binding=1) uniform sampler2D source;
void main() {
    vec2  texel   = vec2(1.0 / targetWidth, 1.0 / targetHeight);
    vec4  center  = texture(source, qt_TexCoord0);
    int   r       = int(clamp(size, 1.0, 64.0));
    vec4  sum     = vec4(0.0);
    float total   = 0.0;
    for (int x = -r; x <= r; x++) {
        for (int y = -r; y <= r; y++) {
            float dist = float(x*x + y*y);
            if (dist > float(r*r)) continue;
            float w   = exp(-dist / float(r * r) * 2.0);
            vec4  s   = texture(source, qt_TexCoord0 + vec2(float(x), float(y)) * texel);
            sum      += s * w;
            total    += w;
        }
    }
    vec4  blurred = sum / max(total, 0.0001);
    float edge    = abs(center.a - blurred.a) * 4.0;
    float mixF    = clamp(edge, 0.0, 1.0);
    float outA    = blurAlpha > 0.5 ? mix(center.a, blurred.a, mixF) : center.a;
    fragColor     = vec4(mix(center.rgb, blurred.rgb, mixF), outA) * qt_Opacity;
}

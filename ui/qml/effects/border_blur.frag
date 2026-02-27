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
    float blurAlpha;
};

void main() {
    vec2 tc = qt_TexCoord0;
    vec4 c = texture(source, tc);

    float dist = min(min(tc.x, 1.0 - tc.x), min(tc.y, 1.0 - tc.y));
    float minSize = min(targetWidth, targetHeight);
    float pxDist = dist * minSize;

    if (size > 0.0 && pxDist < size) {
        float alpha = pxDist / size;
        if (blurAlpha > 0.5) {
            c *= alpha;
        } else {
            c.rgb *= alpha;
        }
    }

    fragColor = c * qt_Opacity;
}

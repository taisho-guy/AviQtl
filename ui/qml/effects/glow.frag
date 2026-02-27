#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float strength;
    float diffusion;
    float threshold;
    float useOriginalColor;
    vec4 glowColor;
    float targetWidth;
    float targetHeight;
};

void main() {
    vec2 tc = qt_TexCoord0;
    vec4 srcColor = texture(source, tc);

    vec4 blurColor = vec4(0.0);
    float totalWeight = 0.0;
    vec2 texel = vec2(1.0 / targetWidth, 1.0 / targetHeight);

    int steps = int(clamp(diffusion * 0.5, 1.0, 10.0));
    float stepSize = diffusion / float(max(1, steps));

    for (int i = -steps; i <= steps; ++i) {
        for (int j = -steps; j <= steps; ++j) {
            vec2 offset = vec2(float(i), float(j)) * stepSize * texel;
            vec4 sampleCol = texture(source, clamp(tc + offset, 0.0, 1.0));

            // Unpremultiply alpha to check actual rgb brightness
            vec3 unmultiplied = sampleCol.a > 0.0 ? sampleCol.rgb / sampleCol.a : vec3(0.0);
            float sampleBright = dot(unmultiplied, vec3(0.299, 0.587, 0.114));

            if (sampleBright >= threshold && sampleCol.a > 0.0) {
                float weight = exp(-(float(i*i + j*j)) / (steps * steps * 0.5 + 0.1));
                blurColor += sampleCol * weight;
                totalWeight += weight;
            }
        }
    }

    if (totalWeight > 0.0) blurColor /= totalWeight;

    vec3 gColor = useOriginalColor > 0.5 ? blurColor.rgb : glowColor.rgb * blurColor.a;

    vec3 outColor = srcColor.rgb + gColor * strength;
    float outAlpha = min(1.0, srcColor.a + blurColor.a * strength);

    fragColor = vec4(outColor * outAlpha, outAlpha) * qt_Opacity;
}

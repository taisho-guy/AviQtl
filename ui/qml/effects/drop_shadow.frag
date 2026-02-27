#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float radius;
    float xOffset;
    float yOffset;
    float strength;
    vec4 shadowColor;
    float targetWidth;
    float targetHeight;
};

void main() {
    vec2 tc = qt_TexCoord0;
    vec2 texel = vec2(1.0 / max(1.0, targetWidth), 1.0 / max(1.0, targetHeight));
    vec2 offsetTc = tc - vec2(xOffset, yOffset) * texel;

    vec4 srcColor = texture(source, tc);
    vec4 shadowAlphaMap = vec4(0.0);

    if (offsetTc.x >= 0.0 && offsetTc.x <= 1.0 && offsetTc.y >= 0.0 && offsetTc.y <= 1.0) {
        if (radius <= 0.0) {
            shadowAlphaMap = texture(source, offsetTc);
        } else {
            float totalWeight = 0.0;
            int steps = int(clamp(radius * 0.5, 1.0, 10.0));
            float stepSize = radius / float(max(1, steps));

            for (int i = -steps; i <= steps; ++i) {
                for (int j = -steps; j <= steps; ++j) {
                    vec2 sampleTc = offsetTc + vec2(float(i), float(j)) * stepSize * texel;
                    if(sampleTc.x >= 0.0 && sampleTc.x <= 1.0 && sampleTc.y >= 0.0 && sampleTc.y <= 1.0) {
                        float weight = exp(-(float(i*i + j*j)) / (steps * steps * 0.5 + 0.1));
                        shadowAlphaMap += texture(source, sampleTc) * weight;
                        totalWeight += weight;
                    }
                }
            }
            if (totalWeight > 0.0) shadowAlphaMap /= totalWeight;
        }
    }

    float sAlpha = shadowAlphaMap.a * strength * shadowColor.a;
    vec3 sColor = shadowColor.rgb * sAlpha;

    float outAlpha = srcColor.a + sAlpha * (1.0 - srcColor.a);
    vec3 outColor = vec3(0.0);
    if (outAlpha > 0.0) {
        outColor = (srcColor.rgb * srcColor.a + sColor * (1.0 - srcColor.a)) / outAlpha;
    }

    fragColor = vec4(outColor * outAlpha, outAlpha) * qt_Opacity;
}

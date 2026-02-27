#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float strength;
    float centerX;
    float centerY;
    float mode;
    float useOriginalColor;
    vec4 flashColor;
    float targetWidth;
    float targetHeight;
};

void main() {
    vec2 tc = qt_TexCoord0;
    vec4 c = texture(source, tc);

    vec2 centerTc = vec2(0.5 + centerX / targetWidth, 0.5 + centerY / targetHeight);
    float dist = distance(tc, centerTc);
    float intensity = exp(-dist * 5.0) * (strength / 10.0);

    vec3 fColor = useOriginalColor > 0.5 ? c.rgb : flashColor.rgb;

    if (mode < 0.5) { 
        c.rgb = c.rgb + fColor * intensity;
        c.a = min(1.0, c.a + intensity * flashColor.a);
    } else if (mode < 1.5) { 
        float backIntensity = intensity * (1.0 - c.a);
        c.rgb = c.rgb + fColor * backIntensity;
        c.a = min(1.0, c.a + backIntensity * flashColor.a);
    } else { 
        c.rgb = fColor * intensity;
        c.a = intensity * flashColor.a;
    }

    fragColor = vec4(c.rgb * c.a, c.a) * qt_Opacity;
}

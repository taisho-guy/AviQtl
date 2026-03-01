#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float brightness;
    float contrast;
    float hue;
    float luminance;
    float saturation;
    float saturate;
};
layout(binding=1) uniform sampler2D source;

vec3 rgb2hsl(vec3 c) {
    float hi = max(max(c.r, c.g), c.b);
    float lo = min(min(c.r, c.g), c.b);
    float d = hi - lo;
    float l = (hi + lo) * 0.5;
    float s = (d < 0.0001) ? 0.0 : d / (1.0 - abs(2.0 * l - 1.0));
    float h = 0.0;
    if (d > 0.0001) {
        if (hi == c.r)      h = fract((c.g - c.b) / d / 6.0);
        else if (hi == c.g) h = ((c.b - c.r) / d + 2.0) / 6.0;
        else                h = ((c.r - c.g) / d + 4.0) / 6.0;
    }
    return vec3(h, s, l);
}
float hue2rgb(float p, float q, float t) {
    if (t < 0.0) t += 1.0;
    if (t > 1.0) t -= 1.0;
    if (t < 1.0/6.0) return p + (q - p) * 6.0 * t;
    if (t < 0.5)     return q;
    if (t < 2.0/3.0) return p + (q - p) * (2.0/3.0 - t) * 6.0;
    return p;
}
vec3 hsl2rgb(vec3 c) {
    float q = c.z < 0.5 ? c.z * (1.0 + c.y) : c.z + c.y - c.z * c.y;
    float p = 2.0 * c.z - q;
    return vec3(hue2rgb(p, q, c.x + 1.0/3.0),
                hue2rgb(p, q, c.x),
                hue2rgb(p, q, c.x - 1.0/3.0));
}
void main() {
    vec4 col  = texture(source, qt_TexCoord0);
    vec3 hsl  = rgb2hsl(col.rgb);
    hsl.x     = fract(hsl.x + hue / 360.0);
    hsl.y     = clamp(hsl.y * (1.0 + saturation), 0.0, 1.0);
    hsl.z     = clamp(hsl.z + brightness + luminance * (1.0 - hsl.z), 0.0, 1.0);
    vec3 rgb  = hsl2rgb(hsl);
    rgb       = clamp((rgb - 0.5) * (1.0 + contrast) + 0.5, 0.0, 1.0);
    if (saturate > 0.5)
        rgb   = clamp(rgb, 16.0/255.0, 235.0/255.0);
    fragColor = vec4(rgb, col.a) * qt_Opacity;
}

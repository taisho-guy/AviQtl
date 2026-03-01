#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float _p0; float _p1; float _p2;
    vec4  sourceColor;
    vec4  targetColor;
    float hueRange;
    float satRange;
    float boundary;
};
layout(binding=1) uniform sampler2D source;

vec3 rgb2hsv(vec3 c) {
    vec4  K = vec4(0.0, -1.0/3.0, 2.0/3.0, -1.0);
    vec4  p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4  q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + 1e-10)),
                d / (q.x + 1e-10), q.x);
}
vec3 hsv2rgb(vec3 c) {
    vec3 p = abs(fract(c.xxx + vec3(1.0, 2.0/3.0, 1.0/3.0)) * 6.0 - 3.0);
    return c.z * mix(vec3(1.0), clamp(p - 1.0, 0.0, 1.0), c.y);
}
void main() {
    vec4  col      = texture(source, qt_TexCoord0);
    vec3  srcHSV   = rgb2hsv(sourceColor.rgb);
    vec3  dstHSV   = rgb2hsv(targetColor.rgb);
    vec3  pixHSV   = rgb2hsv(col.rgb);
    float hueDiff  = abs(fract(pixHSV.x - srcHSV.x + 0.5) - 0.5) * 2.0;
    float satDiff  = abs(pixHSV.y - srcHSV.y);
    float hueR     = hueRange / 360.0;
    float satR     = satRange / 100.0;
    float bd       = max(boundary / 100.0 * 0.2, 0.001);
    float hFactor  = smoothstep(hueR + bd, hueR - bd + 0.001, hueDiff);
    float sFactor  = smoothstep(satR + bd, satR - bd + 0.001, satDiff);
    float factor   = hFactor * sFactor;
    vec3  resultHSV = vec3(
        mix(pixHSV.x, dstHSV.x, factor),
        mix(pixHSV.y, dstHSV.y, factor),
        mix(pixHSV.z, dstHSV.z, factor));
    fragColor = vec4(hsv2rgb(resultHSV), col.a) * qt_Opacity;
}

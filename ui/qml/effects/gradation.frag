#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float _p0; float _p1; float _p2;
    vec4  startColor;
    vec4  endColor;
    float strength;
    float centerX;
    float centerY;
    float angle;
    float gradWidth;
    float shape;
    float targetWidth;
    float targetHeight;
};
layout(binding=1) uniform sampler2D source;
void main() {
    vec4  col    = texture(source, qt_TexCoord0);
    vec2  center = vec2(0.5 + centerX / targetWidth,
                        0.5 - centerY / targetHeight);
    vec2  d      = qt_TexCoord0 - center;
    float asp    = targetWidth / max(targetHeight, 1.0);
    float ang    = radians(angle);
    vec2  dir    = vec2(cos(ang), sin(ang));
    float halfW  = max(0.001, gradWidth * 0.004);
    float t;
    if (shape < 0.5) {
        float proj = dot(d, dir);
        t = clamp((proj + halfW) / (2.0 * halfW), 0.0, 1.0);
    } else if (shape < 1.5) {
        float dist = length(d * vec2(asp, 1.0));
        t = clamp(dist / halfW, 0.0, 1.0);
    } else if (shape < 2.5) {
        vec2 rotD  = vec2(dot(d, vec2(cos(ang), -sin(ang))),
                          dot(d, vec2(sin(ang),  cos(ang))));
        float dist = max(abs(rotD.x), abs(rotD.y / asp));
        t = clamp(dist / halfW, 0.0, 1.0);
    } else {
        float dist = length(d * vec2(asp, 1.0));
        t = 1.0 - clamp(dist / halfW, 0.0, 1.0);
    }
    vec4  grad   = mix(startColor, endColor, t);
    float blend  = grad.a * strength;
    fragColor    = vec4(mix(col.rgb, grad.rgb, blend), col.a) * qt_Opacity;
}

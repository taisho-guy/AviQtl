#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float deformX;
    float deformY;
    float deformScale;
    float deformAspect;
    float deformRotation;
    float centerX;
    float centerY;
    float shapeRot;
    float shapeSize;
    float shapeAspect;
    float shapeBlur;
    float mapType;
    float deformMethod;
    float targetWidth;
    float targetHeight;
};
layout(binding=1) uniform sampler2D source;
const float PI = 3.14159265;

float sdCircle(vec2 p, float r) { return length(p) - r; }
float sdSquare(vec2 p, float r) { return max(abs(p.x), abs(p.y)) - r; }
float sdTriangle(vec2 p, float r) {
    const float k = 1.732050808;
    p.x = abs(p.x) - r;
    p.y = p.y + r / k;
    if (p.x + k * p.y > 0.0) p = vec2(p.x - k*p.y, -k*p.x - p.y) * 0.5;
    p.x -= clamp(p.x, -2.0*r, 0.0);
    return -length(p) * sign(p.y);
}
float sdPentagon(vec2 p, float r) {
    const vec3 k = vec3(0.809016994, 0.587785252, 0.726542528);
    p.x = abs(p.x);
    p -= 2.0*min(dot(vec2(-k.x,k.y),p),0.0)*vec2(-k.x,k.y);
    p -= 2.0*min(dot(vec2( k.x,k.y),p),0.0)*vec2( k.x,k.y);
    p -= vec2(clamp(p.x,-r*k.z,r*k.z),r);
    return length(p)*sign(p.y);
}
float sdStar5(vec2 p, float r) {
    const vec2 k1 = vec2(0.809016994, -0.587785252);
    const vec2 k2 = vec2(-k1.x, k1.y);
    p.x = abs(p.x);
    p -= 2.0*max(dot(k1,p),0.0)*k1;
    p -= 2.0*max(dot(k2,p),0.0)*k2;
    p.x = abs(p.x);
    p.y -= r;
    float rf = 0.45;
    vec2 ba = rf*vec2(-k1.y,k1.x)-vec2(0,1);
    float h = clamp(dot(p,ba)/dot(ba,ba),0.0,r);
    return length(p-ba*h)*sign(p.y*ba.x-p.x*ba.y);
}

void main() {
    vec2  px      = (qt_TexCoord0 - 0.5) * vec2(targetWidth, targetHeight);
    vec2  cpx     = vec2(centerX, -centerY);
    vec2  dp      = px - cpx;
    float cosR    = cos(-shapeRot * PI / 180.0);
    float sinR    = sin(-shapeRot * PI / 180.0);
    vec2  rp      = vec2(cosR*dp.x - sinR*dp.y, sinR*dp.x + cosR*dp.y);
    rp.y         *= clamp(shapeAspect / 100.0, 0.001, 100.0);
    float sdf;
    float sz      = shapeSize;
    if      (mapType < 0.5) sdf = sdCircle(rp, sz);
    else if (mapType < 1.5) sdf = sdSquare(rp, sz);
    else if (mapType < 2.5) sdf = sdTriangle(rp, sz);
    else if (mapType < 3.5) sdf = sdPentagon(rp, sz);
    else                    sdf = sdStar5(rp, sz);
    float blur   = max(shapeBlur, 0.5);
    float inside = clamp(0.5 - sdf / blur, 0.0, 1.0);
    vec2  sampleUV = qt_TexCoord0;
    if (deformMethod < 0.5) {
        sampleUV += vec2(deformX, -deformY) / vec2(targetWidth, targetHeight) * inside;
    } else if (deformMethod < 1.5) {
        float s  = 1.0 + deformScale / 100.0 * inside;
        float ax = 1.0 + (deformAspect / 100.0) * inside;
        vec2  c  = vec2(0.5 + centerX/targetWidth, 0.5 - centerY/targetHeight);
        sampleUV = c + (sampleUV - c) / vec2(s * ax, s / ax);
    } else {
        float angle = deformRotation * PI / 180.0 * inside;
        float cosA  = cos(angle);
        float sinA  = sin(angle);
        vec2  c     = vec2(0.5 + centerX/targetWidth, 0.5 - centerY/targetHeight);
        vec2  dv    = sampleUV - c;
        sampleUV    = c + vec2(cosA*dv.x - sinA*dv.y*targetWidth/targetHeight,
                               sinA*dv.x*targetHeight/targetWidth + cosA*dv.y);
    }
    fragColor = texture(source, clamp(sampleUV, vec2(0.0), vec2(1.0))) * qt_Opacity;
}

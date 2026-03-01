#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140, binding=0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    float transparency;
    float decay;
    float boundary;
    float direction;
    float targetWidth;
    float targetHeight;
};
layout(binding=1) uniform sampler2D source;
void main() {
    vec2  uv     = qt_TexCoord0;
    float gap    = boundary / max(targetHeight, 1.0);
    float isHoriz = (direction > 1.5) ? 1.0 : 0.0;
    float coord   = (isHoriz > 0.5) ? uv.x : uv.y;
    float splitA  = 0.5 - gap * 0.5;
    float splitB  = 0.5 + gap * 0.5;
    vec4  col;
    if (coord <= splitA) {
        vec2 srcUV = uv;
        if (isHoriz < 0.5) srcUV.y = uv.y / splitA;
        else               srcUV.x = uv.x / splitA;
        col = texture(source, clamp(srcUV, vec2(0.0), vec2(1.0)));
    } else if (coord >= splitB) {
        float t    = (coord - splitB) / (1.0 - splitB);
        vec2  srcUV = uv;
        if (isHoriz < 0.5) {
            float flip = (direction < 0.5) ? (1.0 - uv.y) : uv.y;
            srcUV.y = flip / splitA;
        } else {
            float flip = (direction < 2.5) ? (1.0 - uv.x) : uv.x;
            srcUV.x = flip / splitA;
        }
        col = texture(source, clamp(srcUV, vec2(0.0), vec2(1.0)));
        float decayFactor = 1.0 - t * decay / 100.0;
        float alpha = (1.0 - transparency / 100.0) * clamp(decayFactor, 0.0, 1.0);
        col.a *= alpha;
        col.rgb *= col.a;
    } else {
        col = vec4(0.0);
    }
    fragColor = col * qt_Opacity;
}

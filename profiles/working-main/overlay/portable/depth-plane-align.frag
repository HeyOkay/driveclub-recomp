// Frozen CPU/replay fixture only. No production integration.
#version 450 core
#extension GL_EXT_samplerless_texture_functions : require
layout(binding=0,set=0) uniform texture2D in_tex;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 out_color;
bool validDepth(float d) { return d>0.0 && d<=1.0 && !isnan(d) && !isinf(d); }
float slope(float a, float b) {
    if ((a>0.0 && b>0.0) || (a<0.0 && b<0.0))
        return abs(a)<abs(b) ? a : b;
    return 0.0;
}
layout(push_constant) uniform Alignment { vec2 jitter; } alignment;
void main() {
    vec2 J=alignment.jitter;
    ivec2 p=ivec2(gl_FragCoord.xy);
    vec4 center=texelFetch(in_tex,p,0);
    out_color=center;
    if (all(equal(J,vec2(0.0)))) return;
    ivec2 size=textureSize(in_tex,0);
    if (any(lessThanEqual(p,ivec2(0))) || any(greaterThanEqual(p,size-ivec2(1)))) return;
    float c=center.r;
    if (!validDepth(c)) return;
    float l=texelFetch(in_tex,p+ivec2(-1,0),0).r;
    float r=texelFetch(in_tex,p+ivec2( 1,0),0).r;
    float u=texelFetch(in_tex,p+ivec2(0,-1),0).r;
    float d=texelFetch(in_tex,p+ivec2(0, 1),0).r;
    if (!validDepth(l) || !validDepth(r) || !validDepth(u) || !validDepth(d)) return;
    float sx=slope(c-l,r-c);
    float sy=slope(c-u,d-c);
    float result=c+J.x*sx+J.y*sy;
    if (!validDepth(result)) return;
    out_color.r=result;
}

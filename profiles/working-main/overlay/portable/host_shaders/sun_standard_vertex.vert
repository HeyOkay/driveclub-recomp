// SPDX-License-Identifier: GPL-2.0-or-later
// PRIVATE Standard native-atlas experiment: native-footprint-plane-A.
// Private Standard helper variant; linked into native SPIR-V.
// Derived from the authored sun_filter_helpers.h Ultra profile operations.
// Private portable_* globals form the injection ABI; main is stripped at linkage.
#version 450
layout(set=0,binding=8,std430) readonly buffer ssbo_5 { uint data[]; } ssbo_5_1;
uint portable_buf4off;
vec4 ultraLoad(uint row,uint off) {
    uint n=uint(ssbo_5_1.data.length());
    if(off>n || row>n-off || n-off-row<4u) return vec4(uintBitsToFloat(0x7fc00000u));
    return uintBitsToFloat(uvec4(ssbo_5_1.data[off+row],ssbo_5_1.data[off+row+1u],ssbo_5_1.data[off+row+2u],ssbo_5_1.data[off+row+3u]));
}
bool ultraFinite(vec4 x) { return !any(isnan(x))&&!any(isinf(x)); }
layout(location=3) flat out vec4 ultraAffine0;
layout(location=4) flat out vec4 ultraAffine1;
layout(location=5) flat out vec4 ultraAffine2;
layout(location=6) flat out vec4 ultraAffine3;
layout(location=7) flat out vec4 ultraProfile;

void portableVertex() {
    // Native publisher0x798710 table, profilebase+0x120 in this bound global CB.
    ultraAffine0=ultraLoad(140u,portable_buf4off);
    ultraAffine1=ultraLoad(144u,portable_buf4off);
    ultraAffine2=ultraLoad(148u,portable_buf4off);
    ultraAffine3=ultraLoad(152u,portable_buf4off);
    vec4 n0=ultraLoad(156u,portable_buf4off);
    vec4 fade0=ultraLoad(172u,portable_buf4off);
    vec4 reciprocal=ultraLoad(188u,portable_buf4off);
    vec4 dimensions=ultraLoad(192u,portable_buf4off);
    precise float radius0=n0.x*reciprocal.x;
    float world0=radius0/abs(ultraAffine0.x);
    bool valid=ultraFinite(ultraAffine0)&&ultraFinite(ultraAffine1)&&ultraFinite(ultraAffine2)&&ultraFinite(ultraAffine3)&&
        ultraFinite(n0)&&ultraFinite(fade0)&&ultraFinite(reciprocal)&&ultraFinite(dimensions)&&
        all(equal(n0,vec4(n0.x)))&&n0.x>0.0&&reciprocal.x>0.0&&reciprocal.y>0.0&&
        all(equal(dimensions.xy,vec2(9216.0,8192.0)))&&
        ultraAffine0.x>0.0&&ultraAffine0.y<0.0&&fade0.x==0.0&&fade0.y==1.0&&fade0.z>0.0&&fade0.w>1.0&&
        !isnan(world0)&&!isinf(world0)&&world0>0.0;
    ultraProfile=vec4(world0,fade0.z,fade0.w,valid?1.0:0.0);
}
// Retain authored functions for conversion into a SPIR-V linkage library.
void main() { portableVertex(); }

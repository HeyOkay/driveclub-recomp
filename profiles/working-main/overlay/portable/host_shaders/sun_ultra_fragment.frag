// SPDX-License-Identifier: GPL-2.0-or-later
// PRIVATE D ultra: C-derived near-texel floor, full chosen-footprint reference.
// Private Standard helper variant; linked into native SPIR-V.
// Derived from the authored sun_filter_helpers.h receiver-plane operations.
// Private portable_* globals form the injection ABI; main is stripped at linkage.
#version 450
#extension GL_EXT_fragment_shader_barycentric : require
layout(set=0,binding=0,std430) readonly buffer ssbo_1 { uint data[]; } ssbo_1_1;
layout(set=0,binding=2) uniform texture2D fs_img0;
layout(set=0,binding=3) uniform texture2D fs_img1;
layout(set=0,binding=4) uniform sampler fs_samp0;
layout(set=0,binding=5) uniform samplerShadow fs_samp1;
layout(location=0) pervertexEXT in vec4 fs_in_attr0_p[3];
layout(location=1) pervertexEXT in vec4 fs_in_attr1_p[3];
vec2 portable_depthUV;
vec2 portable_cameraUVScale;
float portable_nativeDepth;
float portable_cameraA;
float portable_viewZ;
vec3 portable_ray;
uvec4 portable_affine;
vec2 portable_nativeRadius;
vec2 portable_center;
uint portable_count;
float portable_reference;
vec2 portable_tap;
vec2 portable_effectiveRadius;
vec2 portable_gradient;
bool portable_correctionActive;
float portable_correctedReference;
layout(location=3) flat in vec4 ultraAffine0;
layout(location=4) flat in vec4 ultraAffine1;
layout(location=5) flat in vec4 ultraAffine2;
layout(location=6) flat in vec4 ultraAffine3;
layout(location=7) flat in vec4 ultraProfile;
bool ultraFinite(vec4 x) { return !any(isnan(x))&&!any(isinf(x)); }
// Standard A qualifies the existing native footprint; no radius growth or cascade exclusion.
bool standardQualified(uvec4 selected,vec2 nativeRadius,vec2 center,float signedViewZ,uint count) {
    if(ultraProfile.w!=1.0 || !ultraFinite(ultraProfile) || count!=32u ||
       !all(equal(textureSize(sampler2DShadow(fs_img1,fs_samp1),0),ivec2(18944,21504))) ||
       any(isnan(nativeRadius))||any(isinf(nativeRadius))||any(lessThanEqual(nativeRadius,vec2(0.0)))||
       any(isnan(center))||any(isinf(center))||isnan(signedViewZ)||isinf(signedViewZ)||signedViewZ>=0.0)
        return false;
    vec4 rows[4]=vec4[4](ultraAffine0,ultraAffine1,ultraAffine2,ultraAffine3);
    int split=-1;uint matches=0u;
    for(int i=0;i<4;++i) {
        if(!ultraFinite(rows[i])) return false;
        if(all(equal(floatBitsToUint(rows[i]),selected))) {++matches;split=i;}
    }
    if(matches!=1u) return false;
    vec4 affine=uintBitsToFloat(selected);
    if(affine.x<=0.0||affine.y>=0.0) return false;
    const ivec4 rects[4]=ivec4[4](ivec4(10752,13824,1024,2048),ivec4(10752,8192,2816,5632),ivec4(0,0,10752,21504),ivec4(10752,0,8192,8192));
    ivec4 rect=rects[split];
    vec2 span=nativeRadius*vec2(1.0236150026321411,1.0064400434494019)+1.0/vec2(18944.0,21504.0);
    vec2 lo=vec2(rect.xy)/vec2(18944.0,21504.0);
    vec2 hi=vec2(rect.xy+rect.zw)/vec2(18944.0,21504.0);
    if(any(lessThan(center-span,lo))||any(greaterThan(center+span,hi))) return false;
    return true;
}
// B differs from A only in its selected footprint: the existing publisher-derived
// C0-world softness envelope/range. C0 retains native radius; no new blur coefficient.
vec2 standardExpandedRadius(uvec4 selected,vec2 nativeRadius,vec2 center,float signedViewZ,uint count) {
    // Exact native radius fallback: never change C0/contact or the native far cascade.
    if(ultraProfile.w!=1.0 || !ultraFinite(ultraProfile) || count!=32u ||
       !all(equal(textureSize(sampler2DShadow(fs_img1,fs_samp1),0),ivec2(18944,21504))) ||
       any(isnan(nativeRadius))||any(isinf(nativeRadius))||any(lessThanEqual(nativeRadius,vec2(0.0)))||
       any(isnan(center))||any(isinf(center))||isnan(signedViewZ)||isinf(signedViewZ)||signedViewZ>=0.0)
        return nativeRadius;
    vec4 rows[4]=vec4[4](ultraAffine0,ultraAffine1,ultraAffine2,ultraAffine3);
    int split=-1;uint matches=0u;
    for(int i=0;i<4;++i) {
        if(!ultraFinite(rows[i])) return nativeRadius;
        if(all(equal(floatBitsToUint(rows[i]),selected))) {++matches;split=i;}
    }
    if(matches!=1u || split==0) return nativeRadius;
    vec4 affine=uintBitsToFloat(selected);
    if(affine.x<=0.0||affine.y>=0.0) return nativeRadius;
    float world=nativeRadius.x/abs(affine.x);
    if(isnan(world)||isinf(world)||world<=0.0) return nativeRadius;
    // Preserve the authored ramp shape while keeping FULL softness through >=50m.
    if(ultraProfile.y<=0.0 || ultraProfile.z<=1.0) return nativeRadius;
    float authoredEnd=(ultraProfile.z-1.0)/ultraProfile.y;
    if(isnan(authoredEnd)||isinf(authoredEnd)||authoredEnd<=0.0) return nativeRadius;
    float rangeMultiplier=max(4.0,50.0/authoredEnd);
    if(isnan(rangeMultiplier)||isinf(rangeMultiplier)||rangeMultiplier<=0.0) return nativeRadius;
    precise float scaledZ=signedViewZ/rangeMultiplier;
    precise float ramp=ultraProfile.y*scaledZ;
    precise float rampOffset=ramp+ultraProfile.z;
    float weight=clamp(rampOffset,0.0,1.0);
    // Near-C0 footprint in units of its coarsest world-space atlas texel.
    // All values come from the actual publisher and selected native atlas affine.
    if(ultraAffine0.x<=0.0 || ultraAffine0.y>=0.0 || ultraProfile.x<=0.0) return nativeRadius;
    vec2 atlasDim=vec2(textureSize(sampler2DShadow(fs_img1,fs_samp1),0));
    vec2 nearWorldTexel=1.0/(atlasDim*abs(ultraAffine0.xy));
    vec2 selectedWorldTexel=1.0/(atlasDim*abs(affine.xy));
    float nearCoarsest=max(nearWorldTexel.x,nearWorldTexel.y);
    float selectedCoarsest=max(selectedWorldTexel.x,selectedWorldTexel.y);
    float nearFootprint=ultraProfile.x/nearCoarsest;
    float targetWorld=nearFootprint*selectedCoarsest;
    if(!ultraFinite(vec4(nearWorldTexel,selectedWorldTexel)) || isnan(nearFootprint)||isinf(nearFootprint)||
       isnan(targetWorld)||isinf(targetWorld)||nearCoarsest<=0.0||selectedCoarsest<=0.0||nearFootprint<=0.0||targetWorld<=0.0)return nativeRadius;
    float scale=mix(1.0,max(1.0,targetWorld/world),weight);
    if(scale<=1.0 || isnan(scale)||isinf(scale)) return nativeRadius;
    vec2 expanded=nativeRadius*scale;
    const ivec4 rects[4]=ivec4[4](ivec4(10752,13824,1024,2048),ivec4(10752,8192,2816,5632),ivec4(0,0,10752,21504),ivec4(10752,0,8192,8192));
    ivec4 rect=rects[split];
    // Native32 publisher table maxabs, includes offsets slightly outside unit disk.
    // One texel covers comparison bilinear support plus floating-point roundoff.
    vec2 span=expanded*vec2(1.0236150026321411,1.0064400434494019)+1.0/vec2(18944.0,21504.0);
    vec2 lo=vec2(rect.xy)/vec2(18944.0,21504.0);
    vec2 hi=vec2(rect.xy+rect.zw)/vec2(18944.0,21504.0);
    if(any(lessThan(center-span,lo))||any(greaterThan(center+span,hi))) return nativeRadius;
    return expanded;
}

vec2 legacyUltraRadius(uvec4 selected,vec2 nativeRadius,vec2 center,float signedViewZ,uint count) {
    // Exact native radius fallback: never change C0/contact or the native far cascade.
    if(ultraProfile.w!=1.0 || !ultraFinite(ultraProfile) || count!=32u ||
       !all(equal(textureSize(sampler2DShadow(fs_img1,fs_samp1),0),ivec2(18944,21504))) ||
       any(isnan(nativeRadius))||any(isinf(nativeRadius))||any(lessThanEqual(nativeRadius,vec2(0.0)))||
       any(isnan(center))||any(isinf(center))||isnan(signedViewZ)||isinf(signedViewZ)||signedViewZ>=0.0)
        return nativeRadius;
    vec4 rows[4]=vec4[4](ultraAffine0,ultraAffine1,ultraAffine2,ultraAffine3);
    int split=-1;uint matches=0u;
    for(int i=0;i<4;++i) {
        if(!ultraFinite(rows[i])) return nativeRadius;
        if(all(equal(floatBitsToUint(rows[i]),selected))) {++matches;split=i;}
    }
    if(matches!=1u || split==0) return nativeRadius;
    vec4 affine=uintBitsToFloat(selected);
    if(affine.x<=0.0||affine.y>=0.0) return nativeRadius;
    float world=nativeRadius.x/abs(affine.x);
    if(isnan(world)||isinf(world)||world<=0.0) return nativeRadius;
    // Preserve the authored ramp shape while keeping FULL softness through >=50m.
    if(ultraProfile.y<=0.0 || ultraProfile.z<=1.0) return nativeRadius;
    float authoredEnd=(ultraProfile.z-1.0)/ultraProfile.y;
    if(isnan(authoredEnd)||isinf(authoredEnd)||authoredEnd<=0.0) return nativeRadius;
    float rangeMultiplier=max(4.0,50.0/authoredEnd);
    if(isnan(rangeMultiplier)||isinf(rangeMultiplier)||rangeMultiplier<=0.0) return nativeRadius;
    precise float scaledZ=signedViewZ/rangeMultiplier;
    precise float ramp=ultraProfile.y*scaledZ;
    precise float rampOffset=ramp+ultraProfile.z;
    float weight=clamp(rampOffset,0.0,1.0);
    float scale=mix(1.0,max(1.0,ultraProfile.x/world),weight);
    if(scale<=1.0 || isnan(scale)||isinf(scale)) return nativeRadius;
    vec2 expanded=nativeRadius*scale;
    const ivec4 rects[4]=ivec4[4](ivec4(10752,13824,1024,2048),ivec4(10752,8192,2816,5632),ivec4(0,0,10752,21504),ivec4(10752,0,8192,8192));
    ivec4 rect=rects[split];
    // Native32 publisher table maxabs, includes offsets slightly outside unit disk.
    // One texel covers comparison bilinear support plus floating-point roundoff.
    vec2 span=expanded*vec2(1.0236150026321411,1.0064400434494019)+1.0/vec2(18944.0,21504.0);
    vec2 lo=vec2(rect.xy)/vec2(18944.0,21504.0);
    vec2 hi=vec2(rect.xy+rect.zw)/vec2(18944.0,21504.0);
    if(any(lessThan(center-span,lo))||any(greaterThan(center+span,hi))) return nativeRadius;
    return expanded;
}

// Standard B: receiver-plane correction for the FULL native footprint. No hardware derivatives.
float planeGamma(float n) { float e=uintBitsToFloat(0x33800000u);return (n*e)/(1.0-n*e); }
float planeMinmod(float a,float b) {return ((a>0.0&&b>0.0)||(a<0.0&&b<0.0))?(abs(a)<abs(b)?a:b):0.0;}
bool planeFinite(float x) {return !isnan(x)&&!isinf(x);}
bool planeFinite3(vec3 x) {return !any(isnan(x))&&!any(isinf(x));}
bool ultraReceiverPlane(vec2 depthUV,vec2 cameraUVScale,float nativeDepth,float cameraA,float z,vec3 ray,
                        vec2 atlasScale,vec2 deltaRadius,float originalReference,out vec2 gradient) {
    gradient=vec2(0.0);
    ivec2 size=textureSize(sampler2D(fs_img0,fs_samp0),0);
    if(any(lessThan(size,ivec2(3)))||!ultraFinite(vec4(depthUV,cameraUVScale))||!planeFinite(cameraA)||!planeFinite(z)||!planeFinite3(ray))return false;
    // Float-to-int is defined only after normalized/finite/range proof.
    if(any(greaterThan(size,ivec2(16777215)))||any(lessThanEqual(depthUV,vec2(0.0)))||any(greaterThanEqual(depthUV,vec2(1.0))))return false;
    precise vec2 sourcePixel=depthUV*vec2(size);
    if(!ultraFinite(vec4(sourcePixel,0.0,0.0))||any(lessThan(sourcePixel,vec2(1.0)))||any(greaterThanEqual(sourcePixel,vec2(size-ivec2(1)))))return false;
    ivec2 p=ivec2(floor(sourcePixel));
    if(any(lessThanEqual(p,ivec2(0)))||any(greaterThanEqual(p,size-ivec2(1))))return false;
    float c=texelFetch(sampler2D(fs_img0,fs_samp0),p,0).x;
    float l=texelFetch(sampler2D(fs_img0,fs_samp0),p+ivec2(-1,0),0).x;
    float r=texelFetch(sampler2D(fs_img0,fs_samp0),p+ivec2(1,0),0).x;
    float u=texelFetch(sampler2D(fs_img0,fs_samp0),p+ivec2(0,-1),0).x;
    float d=texelFetch(sampler2D(fs_img0,fs_samp0),p+ivec2(0,1),0).x;
    if(!ultraFinite(vec4(c,l,r,u))||!planeFinite(d)||min(min(c,l),min(r,min(u,d)))<=0.0||max(max(c,l),max(r,max(u,d)))>1.0||c!=nativeDepth)return false;
    float maximum=max(max(c,l),max(r,max(u,d)));int exponent;frexp(maximum,exponent);
    // Power-of-two normalization avoids underflow of curvature/ULP guards at tiny valid depth.
    precise float nc=ldexp(c,-exponent),nl=ldexp(l,-exponent),nr=ldexp(r,-exponent),nu=ldexp(u,-exponent),nd=ldexp(d,-exponent);
    if(min(min(nc,nl),min(nr,min(nu,nd)))<0.5*max(max(nc,nl),max(nr,max(nu,nd))))return false;
    float twiceMaximum=2.0*maximum;
    float storageULP=uintBitsToFloat(floatBitsToUint(twiceMaximum)+1u)-twiceMaximum;
    float U=ldexp(storageULP,-exponent);
    if(!planeFinite(U)||U<=0.0)return false;
    // Each aligned R32 value <=4U error: .5U center +2*(.75U weighted-slope +.5U product-round) +2*.5U add-round.
    // Bilateral second difference has coefficient absolute sum4 ->16U, plus<=U evaluation ->17U.
    precise float gxL=nc-nl,gxR=nr-nc,gyU=nc-nu,gyD=nd-nc;

    precise vec2 depthSlope=vec2(ldexp(planeMinmod(gxL,gxR),exponent),ldexp(planeMinmod(gyU,gyD),exponent));
    float slopeError=ldexp(9.0*U,exponent); // 2*4U input errors plus<=U subtraction.
    // Native per-vertex attr0 -> actual depth UV, ray differences; no gl_FragCoord or dFdx assumptions.
    precise vec2 a=(fs_in_attr0_p[1].xy-fs_in_attr0_p[0].xy)*cameraUVScale;
    precise vec2 b=(fs_in_attr0_p[2].xy-fs_in_attr0_p[0].xy)*cameraUVScale;
    precise vec3 ra=fs_in_attr1_p[1].xyz-fs_in_attr1_p[0].xyz;
    precise vec3 rb=fs_in_attr1_p[2].xyz-fs_in_attr1_p[0].xyz;
    precise float t0=a.x*b.y,t1=a.y*b.x,sourceDet=t0-t1;
    float sourceError=planeGamma(8.0)*(abs(t0)+abs(t1));
    if(!planeFinite(sourceDet)||!planeFinite(sourceError)||abs(sourceDet)<=sourceError)return false;
    precise vec3 nx=ra*b.y-rb*a.y,ny=rb*a.x-ra*b.x;
    precise vec3 rx=nx/sourceDet/float(size.x),ry=ny/sourceDet/float(size.y);
    vec3 nxError=planeGamma(8.0)*(abs(ra*b.y)+abs(rb*a.y));
    vec3 nyError=planeGamma(8.0)*(abs(rb*a.x)+abs(ra*b.x));
    vec3 rxError=(nxError+abs(nx/sourceDet)*sourceError)/(abs(sourceDet)-sourceError)/float(size.x)+planeGamma(2.0)*abs(rx);
    vec3 ryError=(nyError+abs(ny/sourceDet)*sourceError)/(abs(sourceDet)-sourceError)/float(size.y)+planeGamma(2.0)*abs(ry);
    precise vec3 k=cameraA*z*z*ray;
    precise vec3 lx=z*rx-k*depthSlope.x,ly=z*ry-k*depthSlope.y;
    vec3 lxError=abs(z)*rxError+abs(k)*slopeError+planeGamma(8.0)*(abs(z*rx)+abs(k*depthSlope.x));
    vec3 lyError=abs(z)*ryError+abs(k)*slopeError+planeGamma(8.0)*(abs(z*ry)+abs(k*depthSlope.y));
    precise vec2 columnX=lx.xy*atlasScale,columnY=ly.xy*atlasScale;
    vec2 ex=lxError.xy*abs(atlasScale)+planeGamma(1.0)*abs(columnX);
    vec2 ey=lyError.xy*abs(atlasScale)+planeGamma(1.0)*abs(columnY);
    precise float p0=columnX.x*columnY.y,p1=columnY.x*columnX.y,det=p0-p1;
    float determinantError=abs(columnY.y)*ex.x+abs(columnX.x)*ey.y+ex.x*ey.y+
        abs(columnX.y)*ey.x+abs(columnY.x)*ex.y+ey.x*ex.y+planeGamma(3.0)*(abs(p0)+abs(p1));
    if(!planeFinite3(lx)||!planeFinite3(ly)||!planeFinite(det)||!planeFinite(determinantError)||abs(det)<=determinantError)return false;
    precise float gu=(lx.z*columnY.y-ly.z*columnX.y)/det;
    precise float gv=(columnX.x*ly.z-columnY.x*lx.z)/det;
    gradient=vec2(gu,gv);
    if(!ultraFinite(vec4(gradient,0.0,0.0)))return false;
    float bound=dot(abs(gradient),deltaRadius*vec2(1.0236150026321411,1.0064400434494019));
    // 12-op upper bound covers offset products, dot bound and two reference additions.
    float domainAllowance=planeGamma(12.0)*(abs(originalReference)+bound);
    float paddedBound=bound+domainAllowance;
    if(!planeFinite(bound)||!planeFinite(paddedBound)||!planeFinite(originalReference)||originalReference-paddedBound<=0.0||originalReference+paddedBound>=1.0)return false;
    return true;
}


void portablePrepare() {
    // Kernel coefficients, count and reduction remain native; B selects only radius.
    portable_effectiveRadius=portable_nativeRadius;
    portable_correctionActive=false;portable_gradient=vec2(0.0);
    if(standardQualified(portable_affine,portable_nativeRadius,portable_center,portable_viewZ,portable_count)) {
        portable_effectiveRadius=standardExpandedRadius(portable_affine,portable_nativeRadius,portable_center,portable_viewZ,portable_count);
    vec2 legacyRadius=legacyUltraRadius(portable_affine,portable_nativeRadius,portable_center,portable_viewZ,portable_count);
    portable_effectiveRadius=max(portable_effectiveRadius,legacyRadius);
        portable_correctionActive=ultraReceiverPlane(portable_depthUV,portable_cameraUVScale,portable_nativeDepth,portable_cameraA,portable_viewZ,portable_ray,uintBitsToFloat(portable_affine).xy,portable_effectiveRadius,portable_reference,portable_gradient);
        if(!portable_correctionActive) portable_effectiveRadius=portable_nativeRadius;
    }
}
void portableReference() {
    portable_correctedReference=portable_reference;
    if(portable_correctionActive) {
        precise vec2 offset=portable_effectiveRadius*portable_tap;
        precise float correctionX=portable_gradient.x*offset.x;
        precise float correctionY=portable_gradient.y*offset.y;
        precise float correctedX=portable_reference+correctionX;
        portable_correctedReference=correctedX+correctionY;
    }
}
// Retain authored functions for conversion into a SPIR-V linkage library.
void main() { portablePrepare(); portableReference(); }

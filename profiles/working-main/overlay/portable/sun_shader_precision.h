// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

// Derive the precision variant from the caller's native shader. Only identity
// digests and six operand substitutions are published; no native body is stored.
namespace BbSunPrecision {
namespace detail {
using Digest = std::array<std::uint32_t, 8>;
inline constexpr std::array<std::uint32_t, 64> kSha256Round = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};

// Bytes are supplied explicitly so module identity is independent of host endian.
// The caller bounds byte_count to a module size or a small test vector.
template<class ByteAt> inline Digest Sha256(std::size_t byte_count, ByteAt byte_at) {
    Digest h{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    const auto blocks = byte_count / 64 + (byte_count % 64 < 56 ? 1 : 2);
    const auto bit_count = static_cast<std::uint64_t>(byte_count) * 8;
    for (std::size_t block = 0; block < blocks; ++block) {
        std::array<std::uint32_t, 64> w{};
        for (std::size_t j = 0; j < 64; ++j) {
            const auto i = block * 64 + j;
            std::uint32_t value = 0;
            if (i < byte_count) value = byte_at(i);
            else if (i == byte_count) value = 0x80;
            else if (block == blocks - 1 && j >= 56) value = (bit_count >> ((63-j)*8)) & 0xff;
            w[j/4] |= value << ((3-j%4)*8);
        }
        for (std::size_t j=16; j<64; ++j) {
            const auto s0=std::rotr(w[j-15],7)^std::rotr(w[j-15],18)^(w[j-15]>>3);
            const auto s1=std::rotr(w[j-2],17)^std::rotr(w[j-2],19)^(w[j-2]>>10);
            w[j]=w[j-16]+s0+w[j-7]+s1;
        }
        auto [a,b,c,d,e,f,g,h0]=h;
        for (std::size_t j=0; j<64; ++j) {
            const auto s1=std::rotr(e,6)^std::rotr(e,11)^std::rotr(e,25);
            const auto t1=h0+s1+((e&f)^(~e&g))+kSha256Round[j]+w[j];
            const auto s0=std::rotr(a,2)^std::rotr(a,13)^std::rotr(a,22);
            const auto t2=s0+((a&b)^(a&c)^(b&c));
            h0=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=h0;
    }
    return h;
}
inline Digest Sha256Words(std::span<const std::uint32_t> code) {
    return Sha256(code.size_bytes(),[&](std::size_t i) { return (code[i/4] >> ((i%4)*8)) & 0xff; });
}
inline constexpr Digest kNativeVertexDigest{
    0x0a450d5b,0x6193c570,0xf679153b,0xb69f5b4b,0x237843d6,0x03da8852,0xd66c0a50,0x12fc99ff};
inline constexpr Digest kNativeFragmentDigest{
    0xb67cdb7d,0xf7e5beac,0x726c96c9,0xea27aae2,0xb210c01f,0x21c4cc71,0x3c4518dc,0xee2c6cd4};
struct Edit {
    std::uint32_t opcode, type, result, left, old_right, new_right;
};
inline constexpr std::array<Edit,6> kEdits{{
    {129,8,404,403,391,390},{129,8,408,407,398,397},{129,8,411,410,401,400},
    {131,8,424,31,421,414},{131,8,425,31,422,417},{131,8,426,31,423,420}}};

// Internal shape checker, exposed only for source-only structural tests. Public
// RewriteVertex ALWAYS requires the immutable full-module digest first.
// Validate every instruction boundary and every unique edit before mutating.
inline bool PatchOperands(std::span<std::uint32_t> code) {
    if (code.size()<5 || code[0]!=0x07230203 || code[1]!=0x00010600 || code[4]!=0 || code[3]<=426) return false;
    std::array<std::size_t,6> sites{};
    for (std::size_t i=5; i<code.size();) {
        const auto n=code[i]>>16, op=code[i]&0xffff;
        if (n==0 || n>code.size()-i) return false;
        if (op==129 || op==131) {
            if(n!=5) return false;
            for(std::size_t j=0;j<kEdits.size();++j) {
                const auto& e=kEdits[j];
                if(code[i+2]!=e.result) continue;
                if(sites[j]!=0 || op!=e.opcode || code[i+1]!=e.type ||
                   code[i+3]!=e.left || code[i+4]!=e.old_right) return false;
                sites[j]=i+4;
            }
        }
        i+=n;
    }
    for(auto site:sites) if(site==0) return false;
    for(std::size_t j=0;j<sites.size();++j) code[sites[j]]=kEdits[j].new_right;
    return true;
}
} // namespace detail
inline bool IsNativeVertex(std::span<const std::uint32_t> code) {
    return code.size()==2954 && detail::Sha256Words(code)==detail::kNativeVertexDigest;
}
inline bool IsNativeFragment(std::span<const std::uint32_t> code) {
    return code.size()==2623 && detail::Sha256Words(code)==detail::kNativeFragmentDigest;
}
inline std::vector<std::uint32_t> RewriteVertex(std::span<const std::uint32_t> code) {
    if(!IsNativeVertex(code)) return {};
    std::vector<std::uint32_t> result(code.begin(),code.end());
    if(!detail::PatchOperands(result)) return {};
    return result;
}
} // namespace BbSunPrecision

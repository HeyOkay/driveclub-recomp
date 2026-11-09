#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include "bbport_settings.h"
#include "shader_recompiler/draw_control.h"

int main(int argc, char** argv) {
    assert(argc == 2);
    const bool expected = std::strcmp(argv[1], "1") == 0;
    BbSettings::Load(); auto& settings = BbSettings::Get();
    assert(settings.texture_repetition == expected);
    assert(!BbSettings::ResolutionNeedsRestart());
    settings.texture_repetition = !expected;
    assert(!BbSettings::ResolutionNeedsRestart());
    const auto read = [] {std::ifstream f(std::getenv("BB_CONFIG"), std::ios::binary); return std::string(std::istreambuf_iterator<char>(f), {});};
    const auto before = read();
    assert(BbSettings::SaveTextureRepetition(!expected));
    assert(read() == before); // preserve the whole main INI, including unknown options
    for (unsigned motion : {0u,1u,8191u,8192u,32767u,32768u}) for (bool enabled : {false,true}) {
        const auto packed = Shader::DrawControl::Pack(motion,enabled);
        assert(Shader::DrawControl::MotionIndex(packed) == motion);
        assert(Shader::DrawControl::TextureRepeat(packed) == enabled);
    }
    puts("PASS: live sampling, no restart, motion-index isolation, persistence without changing unrelated settings");
}

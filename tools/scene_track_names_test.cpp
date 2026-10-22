#include <windows.h>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include "../launcher/scene_track_names.h"

int wmain(int argc, wchar_t** argv) {
    const auto fields = SceneTrackNames::Fields("\"road,id\",\"A \"\"quoted\"\" name\",,last\r");
    assert(fields.size() == 4 && fields[0] == "road,id");
    assert(fields[1] == "A \"quoted\" name" && fields[2].empty() && fields[3] == "last");
    assert(SceneTrackNames::Utf8("Chungar\xe1 Lake") == "Chungar\xc3\xa1 Lake");
    if (argc == 3) {
        assert(SceneTrackNames::Write(argv[1], L"", argv[2]));
        FILE* file = _wfopen(argv[2], L"rb");
        assert(file);
        std::string output;
        char line[512];
        while (std::fgets(line, sizeof(line), file)) output += line;
        std::fclose(file);
        assert(output.find("chile_road_circuit_02_n\tSalar de Surire\n") != std::string::npos);
    }
}

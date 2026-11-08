#pragma once
#include <string>
#include "graphics.h"

struct SunShadowBundle { std::wstring boot, patches, graphics; int proxy_scale=0; };

// Creates a separate complete cache generation; canonical prep and graphics files
// are never modified. A generation is published only after every component succeeds.
// Non-empty error means the selected quality must not be launched.
std::wstring PrepareSunShadows(const std::wstring& prep, const std::wstring& cache,
                              const std::wstring& update, const std::wstring& base,
                              const std::wstring& runtime, const GraphicsChoice& choice,
                              SunShadowBundle& bundle);

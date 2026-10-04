#pragma once

#include <stdint.h>
#include <string>

// Option icons for the ReShade tab (Phase 2): the data pack's JPEG/PNG
// files, decoded with WIC and uploaded through ReShade's own device, shown
// with ImGui::ImageButton. Only call while holding the menu lock (the tab
// holds it while drawing, like MenuDraw did); results are cached for the
// process. Returns 0 when there is no icon (yet): the caller shows the text
// cell instead. At most *budget new uploads per call, so opening a big list
// does not stutter.

uint64_t TabIcon(void* runtime, const std::wstring& path, unsigned* width, unsigned* height, int* budget);

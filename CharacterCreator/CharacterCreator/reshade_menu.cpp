// The editor as a ReShade addon tab (Phase 0: lifecycle only).
//
// Registers a "Character Creator" tab with ReShade and drives the menu
// session (see MenuSessionBegin/End in menu.cpp) from the ReShade overlay's
// open/close state. Keyboard and mouse reach the tab through ReShade; while
// the tab draws, game input is blocked for the frame.
//
// Every ImGui call below runs inside ReShade's overlay callback, where its
// ImGui function table is valid. Nothing here touches the D2D overlay.

#include "pch.h"
#include "reshade_menu.h"
#include "characters.h"
#include "log.h"
#include "menu.h"

#pragma warning(push, 0)
#include <imgui.h>
#include <reshade.hpp>
#pragma warning(pop)

static HMODULE g_reshadeModule = NULL;
static bool g_reshadeTab = false;
static int g_lastCharacter = CHAR_KLIFF;

// Labels are plain ASCII like CHARACTER_NAMES; narrow copies avoid a
// wide-to-UTF-8 helper until the full menu (Phase 1) needs one.
static const char* const CHARACTER_LABELS[CHARACTER_COUNT] = { "Kliff", "Damiane", "Oongka" };

// Phase 0 tab: proves the tab draws, the session opens with the camera zoom,
// characters switch, and closing keeps the changes.
static void DrawTab(reshade::api::effect_runtime* runtime)
{
    runtime->block_input_next_frame();

    ImGui::TextUnformatted("Character Creator (ReShade tab, Phase 0)");
    ImGui::Separator();

    for (int ch = 0; ch < CHARACTER_COUNT; ++ch)
    {
        if (ch > 0)
            ImGui::SameLine();

        if (ImGui::Button(CHARACTER_LABELS[ch], ImVec2(0, 0)))
        {
            g_lastCharacter = ch;
            MenuSessionBegin(ch);
        }
    }

    ImGui::Text("session: %s", MenuSessionOpen() ? "open" : "closed");

    if (MenuSessionOpen() && ImGui::Button("Keep changes and close", ImVec2(0, 0)))
    {
        MenuSessionEnd(true);
        runtime->open_overlay(false, reshade::api::input_source::keyboard);
    }
}

// The overlay opening for any reason starts the session (the camera zoom is
// the visible proof); closing it keeps the changes, like the hotkey path.
static bool OnOverlayOpenClose(reshade::api::effect_runtime* runtime, bool open,
    reshade::api::input_source source)
{
    (void)runtime;
    (void)source;

    if (open)
    {
        if (!MenuSessionOpen())
            MenuSessionBegin(g_lastCharacter);
    }
    else if (MenuSessionOpen())
        MenuSessionEnd(true);

    return false;   // never vetoes the state change
}

bool ReshadeMenuInit(HMODULE module)
{
    if (!reshade::register_addon(module))
    {
        Log("reshade tab: ReShade not found - the menu needs ReShade with add-on support");
        return false;
    }

    reshade::register_overlay("Character Creator", &DrawTab);
    reshade::register_event<reshade::addon_event::reshade_open_overlay>(&OnOverlayOpenClose);
    g_reshadeModule = module;
    g_reshadeTab = true;
    Log("reshade tab: registered (open ReShade with Home)");
    return true;
}

bool ReshadeMenuActive()
{
    return g_reshadeTab;
}

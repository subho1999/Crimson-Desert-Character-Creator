// The editor as a ReShade addon tab (Phase 1: full menu, text cells).
//
// Registers a "Character Creator" tab with ReShade. The tab draws only while
// selected, so the menu session (and its camera zoom) starts on the first
// drawn frame and never merely because the overlay opened for another tab.
// Closing the overlay keeps the changes; leaving the tab ends the session
// through the watchdog below (ReshadeMenuPoll), also keeping them.
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

// Overlay state for the watchdog: set by the open/close event, draw time by
// the tab callback (which only runs while the tab is visible).
static bool g_overlayOpen = false;
static DWORD g_lastDrawTick = 0;

// Leaving the tab for this long ends the session (kept), restoring the
// camera: switching tabs fires no event, so absence of draws is the signal.
static const DWORD TAB_HIDDEN_TIMEOUT_MS = 2000;

// Keys polled every drawn frame, mirroring the overlay menu's scheme.
static const int POLL_KEYS[] = {
    VK_TAB, 'Q', 'E', VK_OEM_4, VK_OEM_6,
    VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN, 'A', 'D', 'W', 'S',
    'R', VK_RETURN, VK_SPACE, VK_ESCAPE, VK_PRIOR, VK_NEXT,
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
};

static bool SkipForFocus(int vk, bool widget, bool slider)
{
    // A focused widget owns Space/Enter; a focused slider owns the arrows
    // (and PgUp/PgDn) too. Everything else stays on the menu scheme, so a
    // keyboard-only user (never focused anything) keeps full control.
    if ((vk == VK_SPACE || vk == VK_RETURN) && widget)
        return true;

    if (slider && (vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN ||
        vk == 'A' || vk == 'D' || vk == 'W' || vk == 'S' || vk == VK_PRIOR || vk == VK_NEXT))
        return true;

    return false;
}

static void DrawTab(reshade::api::effect_runtime* runtime)
{
    g_lastDrawTick = GetTickCount();

    // Lazy session start: the callback runs only while the tab is visible,
    // so merely opening the overlay for another tab starts nothing.
    if (MenuSessionOpen())
        g_lastCharacter = MenuSessionCharacter();
    else
        MenuSessionBegin(g_lastCharacter);

    runtime->block_input_next_frame();

    bool widget = MenuTabWidgetFocused(false);
    bool slider = MenuTabWidgetFocused(true);

    for (int vk : POLL_KEYS)
    {
        if (SkipForFocus(vk, widget, slider))
            continue;

        if (runtime->is_key_pressed(vk))
        {
            if (MenuSessionKey(vk))
            {
                runtime->open_overlay(false, reshade::api::input_source::keyboard);
                return;
            }
        }
    }

    MenuDrawTab(runtime);
}

// Closing the overlay keeps the session's changes, like the hotkey path.
// ReShade itself closes on Esc while the overlay has focus: still pressed in
// this frame then, Esc keeps its cancel meaning instead.
static bool OnOverlayOpenClose(reshade::api::effect_runtime* runtime, bool open,
    reshade::api::input_source source)
{
    (void)source;

    if (open)
    {
        g_overlayOpen = true;
    }
    else
    {
        g_overlayOpen = false;

        if (MenuSessionOpen())
            MenuSessionEnd(!runtime->is_key_pressed(VK_ESCAPE));
    }

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

void ReshadeMenuPoll()
{
    if (g_overlayOpen && MenuSessionOpen() && GetTickCount() - g_lastDrawTick > TAB_HIDDEN_TIMEOUT_MS)
    {
        Log("reshade tab: hidden, keeping the session's changes");
        MenuSessionEnd(true);
    }
}

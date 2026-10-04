#pragma once

#include <windows.h>

// The editor as a ReShade addon tab (Phase 0: lifecycle only). Registers a
// "Character Creator" tab with ReShade and drives the menu session from the
// overlay's open/close state. All ImGui calls happen inside ReShade's
// overlay callback, where its ImGui function table is valid.
//
// Returns true when the tab is registered (ReShade present). The game-logic
// hooks are unaffected; only presentation/input move here.

bool ReshadeMenuInit(HMODULE module);
bool ReshadeMenuActive();

// Call regularly from the plugin thread: ends the session (kept) when the
// tab has been hidden a while (switching tabs fires no event).
void ReshadeMenuPoll();

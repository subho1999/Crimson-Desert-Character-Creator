#include "pch.h"
#include "menu_data.h"
#include "log.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <unordered_map>

static std::wstring Widen(const char* s)
{
    wchar_t buffer[512];
    MultiByteToWideChar(CP_UTF8, 0, s, -1, buffer, 512);
    return buffer;
}

bool MenuDataLoad(const char* folder, MenuData* out)
{
    std::unordered_map<std::string, std::string> eyeParts;     // head mesh -> eye part
    char path[MAX_PATH];
    sprintf_s(path, "%s\\menu.txt", folder);

    FILE* f = NULL;
    fopen_s(&f, path, "r");

    if (!f)
    {
        Log("ERROR: menu data not found (%s)", path);
        return false;
    }

    memset(out->textureCounts, 0, sizeof(out->textureCounts));
    memset(out->params, 0, sizeof(out->params));
    out->loadedGender = -1;
    out->loadedRace = -1;
    out->markerSlot = -1;
    out->markerCount = 0;

    char line[4096];
    int meshes = 0, colors = 0, params = 0;

    while (fgets(line, sizeof(line), f))
    {
        line[strcspn(line, "\r\n")] = 0;

        if (line[0] == '#' || !line[0])
            continue;

        char kind[16] = { 0 };
        sscanf_s(line, "%15s", kind, (unsigned)sizeof(kind));

        if (strcmp(kind, "mesh") == 0)
        {
            MeshOption m;
            char race[16], icon[260], mesh[260];
            int shown = 1;

            int fields = sscanf_s(line, "mesh %d %d %15s %d %259s %259s %d %d", &m.slot, &m.index[0],
                race, (unsigned)sizeof(race), &shown, icon, (unsigned)sizeof(icon), mesh, (unsigned)sizeof(mesh),
                &m.index[1], &m.index[2]);

            if (fields < 6)
                continue;

            if (fields < 8)
                m.index[1] = m.index[2] = m.index[0];   // older menu data: one order for everyone

            m.race = strcmp(race, "-") == 0 ? "" : race;
            m.shown = shown != 0;
            m.mesh = mesh;

            if (strcmp(icon, "-") != 0)
            {
                char full[MAX_PATH];
                sprintf_s(full, "%s\\icons\\%s", folder, icon);
                m.icon = Widen(full);
            }

            out->meshes.push_back(m);
            ++meshes;
        }
        else if (strcmp(kind, "color") == 0)
        {
            int palette, index, r, g, b, number;
            char name[64];

            if (sscanf_s(line, "color %d %d %d %d %d %63s %d", &palette, &index, &r, &g, &b,
                name, (unsigned)sizeof(name), &number) != 7 || palette < 0 || palette > 255)
                continue;

            std::vector<PaletteColor>& p = out->palettes[palette];

            if ((int)p.size() <= index)
                p.resize(index + 1);

            wchar_t label[96];
            swprintf_s(label, L"%s %d", Widen(name).c_str(), number);
            p[index] = { (uint8_t)r, (uint8_t)g, (uint8_t)b, label, Widen(name), number };
            ++colors;
        }
        else if (strcmp(kind, "param") == 0)
        {
            int index, lo, hi, def, palette;
            char key[64];

            if (sscanf_s(line, "param %d %63s %d %d %d %d", &index, key, (unsigned)sizeof(key),
                &lo, &hi, &def, &palette) != 6 || index < 0 || index >= 250)
                continue;

            out->params[index] = { true, lo, hi, def, palette };
            ++params;
        }
        else if (strcmp(kind, "marker") == 0)
        {
            int slot, count;

            if (sscanf_s(line, "marker %d %d", &slot, &count) == 2 && slot >= 0 && slot < 16 && count > 0)
            {
                out->markerSlot = slot;
                out->markerCount = count;
            }
        }
        else if (strcmp(kind, "loaded") == 0)
        {
            int gender, race;

            if (sscanf_s(line, "loaded %d %d", &gender, &race) == 2 && gender >= 0 && gender < 2 && race >= 0 && race < 4)
            {
                out->loadedGender = gender;
                out->loadedRace = race;
            }
        }
        else if (strcmp(kind, "base") == 0)
        {
            int gender, race;
            char v[6][128];

            if (sscanf_s(line, "base %d %d %127s %127s %127s %127s %127s %127s", &gender, &race,
                v[0], 128, v[1], 128, v[2], 128, v[3], 128, v[4], 128, v[5], 128) == 8 &&
                gender >= 0 && gender < 2 && race >= 0 && race < 4)
            {
                auto text = [](const char* x) { return std::string(strcmp(x, "-") == 0 ? "" : x); };
                out->bases[gender][race] = { true, text(v[0]), text(v[1]), text(v[2]), text(v[3]), text(v[4]), text(v[5]) };
            }
        }
        else if (strcmp(kind, "eyes") == 0)
        {
            char mesh[260], part[260];

            if (sscanf_s(line, "eyes %259s %259s", mesh, (unsigned)sizeof(mesh), part, (unsigned)sizeof(part)) == 2)
                eyeParts[mesh] = part;
        }
        else if (strcmp(kind, "start") == 0)
        {
            int ch = -1, used = 0;

            if (sscanf_s(line, "start %d%n", &ch, &used) == 1 && ch >= 0 && ch < 3)
            {
                const char* p = line + used;
                int i = 0;

                for (; i < 250; ++i)
                {
                    int value = 0, n = 0;

                    if (sscanf_s(p, "%d%n", &value, &n) != 1)
                        break;

                    out->start[ch][i] = (uint8_t)value;
                    p += n;
                }

                out->startKnown[ch] = i == 250;
            }
        }
        else if (strcmp(kind, "own") == 0)
        {
            int ch;
            char v[6][128];

            if (sscanf_s(line, "own %d %127s %127s %127s %127s %127s %127s", &ch,
                v[0], 128, v[1], 128, v[2], 128, v[3], 128, v[4], 128, v[5], 128) == 7 && ch >= 0 && ch < 3)
            {
                auto text = [](const char* x) { return std::string(strcmp(x, "-") == 0 ? "" : x); };
                out->own[ch] = { true, text(v[0]), text(v[1]), text(v[2]), text(v[3]), text(v[4]), text(v[5]) };
            }
        }
        else if (strcmp(kind, "texture") == 0)
        {
            int palette, index;

            if (sscanf_s(line, "texture %d %d", &palette, &index) == 2 && palette >= 0 && palette < 256 &&
                index + 1 > out->textureCounts[palette])
                out->textureCounts[palette] = index + 1;
        }
    }

    fclose(f);

    // Display bands, measured from the RGB (not the shipped names):
    // achromatic by value, chromatic by hue, muted blondes and dark cyans
    // split into their own bands. Display order below.
    static const wchar_t* const COLOR_BANDS[] = {
        L"White", L"Silver", L"Grey", L"Sage", L"Mauve", L"Charcoal", L"Black",
        L"Pink", L"Rose", L"Red", L"Auburn", L"Copper",
        L"Ginger", L"Blonde", L"Ash Blonde", L"Brown", L"Olive", L"Green",
        L"Teal", L"Slate", L"Blue", L"Navy", L"Violet",
    };
    static const int COLOR_BAND_COUNT = sizeof(COLOR_BANDS) / sizeof(COLOR_BANDS[0]);

    auto rgbToHsv = [](uint8_t r, uint8_t g, uint8_t b, double* h, double* s, double* v)
    {
        double rd = r / 255.0, gd = g / 255.0, bd = b / 255.0;
        double mx = rd > gd ? (rd > bd ? rd : bd) : (gd > bd ? gd : bd);
        double mn = rd < gd ? (rd < bd ? rd : bd) : (gd < bd ? gd : bd);
        *v = mx;
        *s = mx == 0.0 ? 0.0 : (mx - mn) / mx;

        if (mx == mn)
        {
            *h = 0.0;
            return;
        }

        double d = mx - mn;

        if (mx == rd)
        {
            *h = (gd - bd) / d;

            if (*h < 0.0)
                *h += 6.0;
        }
        else if (mx == gd)
            *h = (bd - rd) / d + 2.0;
        else
            *h = (rd - gd) / d + 4.0;

        *h *= 60.0;
    };

    auto bandForColor = [&](uint8_t r, uint8_t g, uint8_t b)
    {
        double h, s, v;
        rgbToHsv(r, g, b, &h, &s, &v);

        // Muted tinted greys read as their tint, not as grey: sage greens
        // and mauves get their own bands (mid-dark only, so bright dusty
        // pastels stay in their hue families). Checked before the grey axis
        // so green/mauve-tinted greys land here instead of Grey.
        if (h >= 85.0 && h < 175.0 && s < 0.30 && v < 0.75) return 3;
        if (h >= 260.0 && h < 340.0 && s < 0.30 && v < 0.75) return 4;

        if (s < 0.16)
        {
            if (v >= 0.85) return 0;    // White
            if (v >= 0.66) return 1;    // Silver
            if (v >= 0.37) return 2;    // Grey
            if (v >= 0.15) return 5;    // Charcoal
            return 6;                   // Black
        }

        if (h >= 328.0) return 7;
        if (h >= 300.0) return 8;
        if (h < 9.0) return 9;
        if (h < 18.0) return 10;
        if (h < 30.0) return 11;
        if (h < 44.0) return s >= 0.32 ? 12 : 14;
        if (h < 62.0)
        {
            if (s < 0.32) return 14;
            if (v >= 0.62) return 13;
            return s >= 0.70 ? 15 : 16;
        }
        if (h < 172.0) return 17;
        if (h < 208.0) return (v >= 0.5 && s >= 0.45) ? 18 : 19;
        if (h < 240.0) return 20;
        if (h < 262.0) return 21;
        return 22;
    };

    // Display order per palette (position -> stored index): bands above,
    // brightest first within each (measured Rec.601 luma). Stored indices
    // never move, so profiles, saves and the game keep working; only the
    // presentation groups.
    for (int pal = 0; pal < 256; ++pal)
    {
        std::vector<PaletteColor>& p = out->palettes[pal];
        std::vector<int>& order = out->paletteOrder[pal];
        std::vector<int> familyRank(p.size(), 0);

        for (size_t i = 0; i < p.size(); ++i)
        {
            int band = bandForColor(p[i].r, p[i].g, p[i].b);

            if (band < 0 || band >= COLOR_BAND_COUNT)
                band = 2;

            p[i].family = COLOR_BANDS[band];
            familyRank[i] = band;
            order.push_back((int)i);
        }

        auto luma = [&](int i) { return 299 * p[i].r + 587 * p[i].g + 114 * p[i].b; };
        std::stable_sort(order.begin(), order.end(), [&](int a, int b)
        {
            if (familyRank[a] != familyRank[b])
                return familyRank[a] < familyRank[b];

            return luma(a) > luma(b);
        });

        // Display names follow the display order (family + position in it),
        // so "Ash 1" is always the brightest Ash shown. Stored indices and
        // the shipped names are untouched.
        std::unordered_map<std::wstring, int> sequence;

        for (int stored_i : order)
        {
            PaletteColor& c = p[stored_i];
            wchar_t label[96];
            swprintf_s(label, L"%s %d", c.family.c_str(), ++sequence[c.family]);
            c.displayName = label;
        }
    }

    for (MeshOption& m : out->meshes)
    {
        auto part = eyeParts.find(m.mesh);

        if (part != eyeParts.end())
            m.eyes = part->second;
    }

    Log("menu data: %d mesh options, %d colours, %d parameters", meshes, colors, params);
    return meshes > 0;
}

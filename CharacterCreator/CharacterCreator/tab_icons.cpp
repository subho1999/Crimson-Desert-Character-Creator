// Option icons for the ReShade tab: WIC decode, ReShade upload.
//
// The D2D overlay decoded through its own device (OverlayLoadImage); the tab
// has none of its own, so icons go through ReShade's device instead, in the
// back buffer's-agnostic B8G8R8A8 straight-alpha form ImGui expects. Decode
// is CPU-only WIC (no GPU interop), which is why it works wherever ReShade
// itself runs, Proton included.

#include "pch.h"
#include "tab_icons.h"
#include "log.h"

#include <unordered_map>
#include <vector>
#include <wincodec.h>

#pragma warning(push, 0)
#include <reshade.hpp>
#pragma warning(pop)

struct Icon
{
    reshade::api::resource resource{ 0 };
    reshade::api::resource_view view{ 0 };
    UINT width = 0, height = 0;
    bool failed = false;    // decode or upload failed: don't retry every frame
};

static std::unordered_map<std::wstring, Icon> s_icons;
static IWICImagingFactory* s_wic = NULL;
static bool s_wicTried = false;

template <typename T> static void ReleaseWic(T*& p)
{
    if (p)
    {
        p->Release();
        p = NULL;
    }
}

static IWICImagingFactory* WicFactory()
{
    if (!s_wic && !s_wicTried)
    {
        s_wicTried = true;
        CoInitializeEx(NULL, COINIT_MULTITHREADED);
        CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&s_wic));

        if (!s_wic)
            Log("tab icons: WIC unavailable");
    }

    return s_wic;
}

static bool DecodeBGRA(const wchar_t* path, UINT* width, UINT* height, std::vector<BYTE>* pixels)
{
    IWICImagingFactory* factory = WicFactory();

    if (!factory)
        return false;

    IWICBitmapDecoder* decoder = NULL;
    IWICBitmapFrameDecode* frame = NULL;
    IWICFormatConverter* converter = NULL;

    bool ok = SUCCEEDED(factory->CreateDecoderFromFilename(path, NULL, GENERIC_READ,
            WICDecodeMetadataCacheOnLoad, &decoder)) &&
        SUCCEEDED(decoder->GetFrame(0, &frame)) &&
        SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
        SUCCEEDED(converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone,
            NULL, 0.0, WICBitmapPaletteTypeCustom)) &&
        SUCCEEDED(converter->GetSize(width, height)) && *width > 0 && *height > 0;

    if (ok)
    {
        pixels->resize((size_t)*width * *height * 4);
        ok = SUCCEEDED(converter->CopyPixels(NULL, *width * 4, (UINT)pixels->size(), pixels->data()));
    }

    ReleaseWic(converter);
    ReleaseWic(frame);
    ReleaseWic(decoder);
    return ok;
}

uint64_t TabIcon(void* runtimePtr, const std::wstring& path, unsigned* width, unsigned* height, int* budget)
{
    *width = *height = 0;

    if (path.empty())
        return 0;

    auto cached = s_icons.find(path);

    if (cached != s_icons.end())
    {
        if (cached->second.failed || cached->second.view == 0)
            return 0;

        *width = cached->second.width;
        *height = cached->second.height;
        return cached->second.view.handle;
    }

    if (*budget <= 0)
        return 0;

    --*budget;
    Icon icon;
    std::vector<BYTE> pixels;

    if (!DecodeBGRA(path.c_str(), &icon.width, &icon.height, &pixels))
    {
        Log("tab icons: cannot decode %S", path.c_str());
        icon.failed = true;
        s_icons[path] = icon;
        return 0;
    }

    reshade::api::effect_runtime* runtime = (reshade::api::effect_runtime*)runtimePtr;
    reshade::api::command_queue* queue = runtime ? runtime->get_command_queue() : NULL;
    reshade::api::device* device = queue ? queue->get_device() : NULL;

    if (!device)
    {
        Log("tab icons: no ReShade device");
        icon.failed = true;
        s_icons[path] = icon;
        return 0;
    }

    reshade::api::resource_desc desc(icon.width, icon.height, 1, 1,
        reshade::api::format::b8g8r8a8_unorm, 1,
        reshade::api::memory_heap::gpu_only, reshade::api::resource_usage::shader_resource);
    reshade::api::subresource_data initial = { pixels.data(), icon.width * 4, 0 };

    if (!device->create_resource(desc, &initial, reshade::api::resource_usage::shader_resource, &icon.resource))
    {
        Log("tab icons: cannot create texture for %S", path.c_str());
        icon.failed = true;
        s_icons[path] = icon;
        return 0;
    }

    reshade::api::resource_view_desc view(reshade::api::format::b8g8r8a8_unorm);

    if (!device->create_resource_view(icon.resource, reshade::api::resource_usage::shader_resource,
            view, &icon.view))
    {
        Log("tab icons: cannot create view for %S", path.c_str());
        device->destroy_resource(icon.resource);
        icon.failed = true;
        s_icons[path] = icon;
        return 0;
    }

    *width = icon.width;
    *height = icon.height;
    s_icons[path] = icon;
    return icon.view.handle;
}

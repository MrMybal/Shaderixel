#include "io/GifWriter.h"

#include <windows.h>
#include <wincodec.h>
#include <propvarutil.h>

#include <algorithm>
#include <vector>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "propsys.lib")

namespace sx {

namespace {
template <class T> void Release(T*& p) {
    if (p) p->Release();
    p = nullptr;
}
}  // namespace

struct GifWriter::Impl {
    IWICImagingFactory* factory = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    u32 fps = 30, maxWidth = 720;
    u16 delay = 3;  // 1/100 s per frame
    double carry = 0;
    std::vector<u8> bgra;
};

bool GifWriter::Open(const std::string& path, u32 fps, u32 maxWidth, std::string* error, u32 loops) {
    Close();
    auto fail = [&](const char* what, HRESULT hr) {
        if (error) {
            char b[96];
            snprintf(b, sizeof(b), "%s (0x%08X)", what, (unsigned)hr);
            *error = b;
        }
        Close();
        return false;
    };
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    impl = new Impl;
    impl->fps = std::max(fps, 1u);
    impl->maxWidth = std::max(maxWidth, 16u);
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&impl->factory));
    if (FAILED(hr)) return fail("WIC indisponible", hr);
    hr = impl->factory->CreateStream(&impl->stream);
    if (SUCCEEDED(hr)) hr = impl->stream->InitializeFromFilename(Widen(path).c_str(), GENERIC_WRITE);
    if (FAILED(hr)) return fail("création du fichier GIF impossible", hr);
    hr = impl->factory->CreateEncoder(GUID_ContainerFormatGif, nullptr, &impl->encoder);
    if (SUCCEEDED(hr)) hr = impl->encoder->Initialize(impl->stream, WICBitmapEncoderNoCache);
    if (FAILED(hr)) return fail("encodeur GIF indisponible", hr);
    // Endless loop (NETSCAPE2.0 application extension).
    IWICMetadataQueryWriter* meta = nullptr;
    if (SUCCEEDED(impl->encoder->GetMetadataQueryWriter(&meta)) && meta) {
        PROPVARIANT v;
        PropVariantInit(&v);
        static UCHAR app[] = {'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0'};
        v.vt = VT_UI1 | VT_VECTOR;
        v.caub.cElems = sizeof(app);
        v.caub.pElems = app;
        meta->SetMetadataByName(L"/appext/Application", &v);
        // Repeat count after the first play (0: endless).
        const u32 rep = loops == 0 ? 0 : std::min(loops - 1, 65535u);
        UCHAR loop[] = {3, 1, (UCHAR)(rep & 0xff), (UCHAR)(rep >> 8), 0};
        v.caub.cElems = sizeof(loop);
        v.caub.pElems = loop;
        meta->SetMetadataByName(L"/appext/Data", &v);
        Release(meta);
    }
    return true;
}

bool GifWriter::Write(const u8* rgba, u32 width, u32 height) { return Write(rgba, width, height, -1.0); }

bool GifWriter::Write(const u8* rgba, u32 width, u32 height, double seconds) {
    if (!impl || !rgba || !width || !height) return false;
    // Size: reduced to maxWidth (box filter over the covered source pixels).
    const u32 W = std::min(width, impl->maxWidth), H = std::max(1u, (u32)((u64)height * W / width));
    impl->bgra.resize((size_t)W * H * 4);
    for (u32 y = 0; y < H; ++y)
        for (u32 x = 0; x < W; ++x) {
            const u32 x0 = x * width / W, x1 = std::max(x0 + 1, (x + 1) * width / W), y0 = y * height / H, y1 = std::max(y0 + 1, (y + 1) * height / H);
            u32 acc[3] = {}, n = 0;
            for (u32 sy = y0; sy < y1; ++sy)
                for (u32 sx = x0; sx < x1; ++sx, ++n) {
                    const u8* p = rgba + ((size_t)sy * width + sx) * 4;
                    acc[0] += p[2];
                    acc[1] += p[1];
                    acc[2] += p[0];
                }
            u8* o = &impl->bgra[((size_t)y * W + x) * 4];
            o[0] = (u8)(acc[0] / n);
            o[1] = (u8)(acc[1] / n);
            o[2] = (u8)(acc[2] / n);
            o[3] = 255;
        }
    IWICBitmap* bmp = nullptr;
    IWICPalette* pal = nullptr;
    IWICFormatConverter* conv = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    IPropertyBag2* props = nullptr;
    HRESULT hr = impl->factory->CreateBitmapFromMemory(W, H, GUID_WICPixelFormat32bppBGRA, W * 4, (UINT)impl->bgra.size(), impl->bgra.data(), &bmp);
    if (SUCCEEDED(hr)) hr = impl->factory->CreatePalette(&pal);
    if (SUCCEEDED(hr)) hr = pal->InitializeFromBitmap(bmp, 256, FALSE);  // optimal palette of this frame
    if (SUCCEEDED(hr)) hr = impl->factory->CreateFormatConverter(&conv);
    if (SUCCEEDED(hr)) hr = conv->Initialize(bmp, GUID_WICPixelFormat8bppIndexed, WICBitmapDitherTypeErrorDiffusion, pal, 0.0, WICBitmapPaletteTypeCustom);
    if (SUCCEEDED(hr)) hr = impl->encoder->CreateNewFrame(&frame, &props);
    if (SUCCEEDED(hr)) hr = frame->Initialize(props);
    if (SUCCEEDED(hr)) hr = frame->SetSize(W, H);
    WICPixelFormatGUID fmt = GUID_WICPixelFormat8bppIndexed;
    if (SUCCEEDED(hr)) hr = frame->SetPixelFormat(&fmt);
    if (SUCCEEDED(hr)) hr = frame->SetPalette(pal);
    if (SUCCEEDED(hr)) {
        // Frame delay in 1/100 s; the remainder carried so the mean rate stays right (e.g. 30 fps: 3, 3, 4).
        impl->carry += seconds >= 0 ? seconds * 100.0 : 100.0 / impl->fps;
        const u16 delay = (u16)std::max(2.0, std::floor(impl->carry));
        impl->carry -= delay;
        IWICMetadataQueryWriter* meta = nullptr;
        if (SUCCEEDED(frame->GetMetadataQueryWriter(&meta)) && meta) {
            PROPVARIANT v;
            PropVariantInit(&v);
            v.vt = VT_UI2;
            v.uiVal = delay;
            meta->SetMetadataByName(L"/grctlext/Delay", &v);
            v.vt = VT_UI1;
            v.bVal = 1;  // do not dispose
            meta->SetMetadataByName(L"/grctlext/Disposal", &v);
            Release(meta);
        }
        hr = frame->WriteSource(conv, nullptr);
    }
    if (SUCCEEDED(hr)) hr = frame->Commit();
    Release(props);
    Release(frame);
    Release(conv);
    Release(pal);
    Release(bmp);
    return SUCCEEDED(hr);
}

bool GifWriter::Close() {
    if (!impl) return true;
    HRESULT hr = S_OK;
    if (impl->encoder) hr = impl->encoder->Commit();
    Release(impl->encoder);
    Release(impl->stream);
    Release(impl->factory);
    delete impl;
    impl = nullptr;
    return SUCCEEDED(hr);
}

}  // namespace sx

#include "io/WicLoad.h"
#include "core/Deflate.h"

#include <cstdio>
#include <future>

#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#pragma comment(lib, "windowscodecs.lib")

namespace sx {

using Microsoft::WRL::ComPtr;

bool IsRawExtension(const std::string& e) {
    static const char* raw[] = {".dng", ".cr2", ".cr3", ".crw", ".nef", ".nrw", ".arw", ".srf", ".sr2", ".orf", ".rw2", ".raf",
                                ".pef", ".srw", ".x3f", ".3fr", ".erf", ".kdc", ".mrw", ".mos", ".iiq", ".rwl",
                                ".raw", ".rwz", ".mef", ".dcr", ".dcs", ".k25", ".cap", ".fff", ".mfw", ".ari", ".bay", ".gpr", ".mdc", ".ptx", ".pxn", ".cs1", ".sti"};
    for (auto* r : raw) if (e == r) return true;
    return false;
}

bool LoadWicFloat(const std::string& path, std::vector<float>& rgba, int& w, int& h, std::string* error) {
    HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    (void)co;  // S_FALSE / RPC_E_CHANGED_MODE: COM already usable on this thread
    auto fail = [&](const char* what, HRESULT hr) {
        if (error) {
            char b[160];
            snprintf(b, sizeof(b), "%s (0x%08lX)", what, (unsigned long)hr);
            *error = b;
        }
        return false;
    };
    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return fail("WIC indisponible", hr);
    ComPtr<IWICBitmapDecoder> dec;
    hr = factory->CreateDecoderFromFilename(Widen(path).c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec);
    if (FAILED(hr)) return fail("aucun décodeur Windows pour ce format (installez l'extension « Raw Image Extension » pour les RAW)", hr);
    ComPtr<IWICBitmapFrameDecode> frame;
    hr = dec->GetFrame(0, &frame);
    if (FAILED(hr)) return fail("image illisible", hr);
    ComPtr<IWICBitmapSource> conv;
    // 128 bpp float RGBA is scene linear (scRGB gamma 1.0).
    hr = WICConvertBitmapSource(GUID_WICPixelFormat128bppRGBAFloat, frame.Get(), &conv);
    if (FAILED(hr)) return fail("conversion impossible", hr);
    UINT uw = 0, uh = 0;
    conv->GetSize(&uw, &uh);
    if (!uw || !uh || uw > 32768 || uh > 32768) return fail("taille non prise en charge", E_FAIL);
    rgba.resize((size_t)uw * uh * 4);
    hr = conv->CopyPixels(nullptr, uw * 16, (UINT)(rgba.size() * sizeof(float)), (BYTE*)rgba.data());
    if (FAILED(hr)) return fail("lecture des pixels impossible", hr);
    w = (int)uw;
    h = (int)uh;
    return true;
}

bool SaveTiff16(const std::string& path, const u16* rgba, int w, int h, bool alpha, std::string* error) {
    const int spp = alpha ? 4 : 3;
    const int rowsPerStrip = std::max(1, std::min(h, (int)(262144 / std::max<size_t>(1, (size_t)w * spp * 2))));
    const int strips = (h + rowsPerStrip - 1) / rowsPerStrip;
    // Strips: predictor 2 (difference with the previous pixel, per channel), deflate; compressed in parallel.
    std::vector<std::vector<u8>> packed(strips);
    {
        std::vector<std::future<void>> jobs;
        const int workers = std::max(1u, std::min(16u, std::thread::hardware_concurrency()));
        std::atomic<int> next{0};
        for (int t = 0; t < workers; ++t)
            jobs.push_back(std::async(std::launch::async, [&] {
                std::vector<u16> buf;
                for (int s; (s = next++) < strips;) {
                    const int y0 = s * rowsPerStrip, y1 = std::min(h, y0 + rowsPerStrip);
                    buf.resize((size_t)(y1 - y0) * w * spp);
                    for (int y = y0; y < y1; ++y) {
                        u16* row = &buf[(size_t)(y - y0) * w * spp];
                        const u16* src = rgba + (size_t)y * w * 4;
                        for (int x = w - 1; x >= 0; --x)
                            for (int c = 0; c < spp; ++c) {
                                const u16 cur = src[(size_t)x * 4 + c], prev = x ? src[(size_t)(x - 1) * 4 + c] : 0;
                                row[(size_t)x * spp + c] = (u16)(cur - prev);
                            }
                    }
                    packed[s] = deflate::ZlibCompress((const u8*)buf.data(), buf.size() * 2, 4);
                }
            }));
        for (auto& j : jobs) j.get();
    }
    FILE* f = nullptr;
    if (_wfopen_s(&f, Widen(path).c_str(), L"wb") != 0 || !f) {
        if (error) *error = "écriture impossible";
        return false;
    }
    auto put16 = [&](u16 v) { fwrite(&v, 2, 1, f); };
    auto put32 = [&](u32 v) { fwrite(&v, 4, 1, f); };
    // Layout: header, strips, then the arrays and the IFD.
    fwrite("II*\0", 1, 4, f);
    put32(0);  // IFD offset, patched at the end
    std::vector<u32> offsets(strips), counts(strips);
    u32 pos = 8;
    for (int s = 0; s < strips; ++s) {
        offsets[s] = pos;
        counts[s] = (u32)packed[s].size();
        fwrite(packed[s].data(), 1, packed[s].size(), f);
        pos += counts[s];
        if (pos & 1) { fputc(0, f); ++pos; }
    }
    const u32 bpsAt = pos;
    for (int c = 0; c < spp; ++c) put16(16);
    pos += spp * 2;
    const u32 resAt = pos;
    put32(72); put32(1);
    pos += 8;
    const u32 offAt = pos;
    for (u32 o : offsets) put32(o);
    pos += strips * 4;
    const u32 cntAt = pos;
    for (u32 c : counts) put32(c);
    pos += strips * 4;
    const u32 ifdAt = pos;
    struct Tag { u16 id, type; u32 count, value; };
    std::vector<Tag> tags = {
        {256, 4, 1, (u32)w}, {257, 4, 1, (u32)h}, {258, 3, (u32)spp, bpsAt}, {259, 3, 1, 8}, {262, 3, 1, 2},
        {273, 4, (u32)strips, strips == 1 ? offsets[0] : offAt}, {277, 3, 1, (u32)spp}, {278, 4, 1, (u32)rowsPerStrip},
        {279, 4, (u32)strips, strips == 1 ? counts[0] : cntAt}, {282, 5, 1, resAt}, {283, 5, 1, resAt}, {284, 3, 1, 1}, {296, 3, 1, 2}, {317, 3, 1, 2}};
    if (alpha) tags.push_back({338, 3, 1, 2});  // unassociated alpha
    put16((u16)tags.size());
    for (auto& t : tags) {
        put16(t.id);
        put16(t.type);
        put32(t.count);
        if (t.type == 3 && t.count == 1) { put16((u16)t.value); put16(0); }
        else put32(t.value);
    }
    put32(0);
    fseek(f, 4, SEEK_SET);
    put32(ifdAt);
    const bool ok = ferror(f) == 0;
    fclose(f);
    if (!ok && error) *error = "erreur d'écriture";
    return ok;
}

bool SavePng16(const std::string& path, const std::vector<u16>& rgba, int w, int h, std::string* error) {
    HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    (void)co;
    auto fail = [&](const char* what, HRESULT hr) {
        if (error) {
            char b[160];
            snprintf(b, sizeof(b), "%s (0x%08lX)", what, (unsigned long)hr);
            *error = b;
        }
        return false;
    };
    if (rgba.size() < (size_t)w * h * 4) return fail("taille", E_INVALIDARG);
    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return fail("WIC indisponible", hr);
    ComPtr<IWICStream> stream;
    hr = factory->CreateStream(&stream);
    if (SUCCEEDED(hr)) hr = stream->InitializeFromFilename(Widen(path).c_str(), GENERIC_WRITE);
    if (FAILED(hr)) return fail("écriture impossible", hr);
    ComPtr<IWICBitmapEncoder> enc;
    hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc);
    if (SUCCEEDED(hr)) hr = enc->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    ComPtr<IWICBitmapFrameEncode> frame;
    if (SUCCEEDED(hr)) hr = enc->CreateNewFrame(&frame, nullptr);
    if (SUCCEEDED(hr)) hr = frame->Initialize(nullptr);
    if (SUCCEEDED(hr)) hr = frame->SetSize((UINT)w, (UINT)h);
    WICPixelFormatGUID fmt = GUID_WICPixelFormat64bppRGBA;
    if (SUCCEEDED(hr)) hr = frame->SetPixelFormat(&fmt);
    if (SUCCEEDED(hr) && fmt != GUID_WICPixelFormat64bppRGBA) hr = E_FAIL;
    if (SUCCEEDED(hr)) hr = frame->WritePixels((UINT)h, (UINT)w * 8, (UINT)(rgba.size() * 2), (BYTE*)rgba.data());
    if (SUCCEEDED(hr)) hr = frame->Commit();
    if (SUCCEEDED(hr)) hr = enc->Commit();
    if (FAILED(hr)) return fail("encodage PNG 16 bits", hr);
    return true;
}

}  // namespace sx

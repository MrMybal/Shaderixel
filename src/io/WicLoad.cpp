#include "io/WicLoad.h"

#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#pragma comment(lib, "windowscodecs.lib")

namespace sx {

using Microsoft::WRL::ComPtr;

bool IsRawExtension(const std::string& e) {
    static const char* raw[] = {".dng", ".cr2", ".cr3", ".crw", ".nef", ".nrw", ".arw", ".srf", ".sr2", ".orf", ".rw2", ".raf",
                                ".pef", ".srw", ".x3f", ".3fr", ".erf", ".kdc", ".mrw", ".mos", ".iiq", ".rwl"};
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

}  // namespace sx

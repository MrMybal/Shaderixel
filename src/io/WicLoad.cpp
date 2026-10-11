#include "io/WicLoad.h"

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

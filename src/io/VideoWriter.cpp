#include "io/VideoWriter.h"

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

#include <algorithm>
#include <vector>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace sx {

struct VideoWriter::Impl {
    IMFSinkWriter* writer = nullptr;
    DWORD stream = 0;
    u32 w = 0, h = 0, fps = 30;
    LONGLONG frame = 0;
    std::vector<u8> bgra;
};

namespace {
template <class T> void SafeRelease(T*& p) {
    if (p) p->Release();
    p = nullptr;
}
}  // namespace

bool VideoWriter::Open(const std::string& path, u32 width, u32 height, u32 fps, std::string* error) {
    Close();
    auto fail = [&](const char* what, HRESULT hr) {
        if (error) {
            char b[96];
            snprintf(b, sizeof(b), "%s (0x%08X)", what, (unsigned)hr);
            *error = b;
        }
        return false;
    };
    width &= ~1u;
    height &= ~1u;
    if (width < 16 || height < 16) return fail("taille trop petite", E_INVALIDARG);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    if (FAILED(hr)) return fail("Media Foundation indisponible", hr);
    IMFAttributes* attr = nullptr;
    MFCreateAttributes(&attr, 2);
    attr->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    attr->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);
    IMFSinkWriter* writer = nullptr;
    hr = MFCreateSinkWriterFromURL(Widen(path).c_str(), nullptr, attr, &writer);
    SafeRelease(attr);
    if (FAILED(hr)) {
        MFShutdown();
        return fail("création du fichier vidéo impossible", hr);
    }
    // Output: H.264, about 0.2 bit per pixel and frame (clean for renders).
    IMFMediaType* out = nullptr;
    MFCreateMediaType(&out);
    out->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    out->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    out->SetUINT32(MF_MT_AVG_BITRATE, (UINT32)std::min<u64>((u64)width * height * fps / 5, 80000000ull));
    out->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    out->SetUINT32(MF_MT_MPEG2_PROFILE, 100);  // High
    MFSetAttributeSize(out, MF_MT_FRAME_SIZE, width, height);
    MFSetAttributeRatio(out, MF_MT_FRAME_RATE, fps, 1);
    MFSetAttributeRatio(out, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    DWORD stream = 0;
    hr = writer->AddStream(out, &stream);
    SafeRelease(out);
    if (FAILED(hr)) {
        SafeRelease(writer);
        MFShutdown();
        return fail("encodeur H.264 indisponible", hr);
    }
    // Input: 32-bit BGRA, top-down (positive stride).
    IMFMediaType* in = nullptr;
    MFCreateMediaType(&in);
    in->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    in->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    in->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    in->SetUINT32(MF_MT_DEFAULT_STRIDE, width * 4);
    MFSetAttributeSize(in, MF_MT_FRAME_SIZE, width, height);
    MFSetAttributeRatio(in, MF_MT_FRAME_RATE, fps, 1);
    MFSetAttributeRatio(in, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    hr = writer->SetInputMediaType(stream, in, nullptr);
    SafeRelease(in);
    if (SUCCEEDED(hr)) hr = writer->BeginWriting();
    if (FAILED(hr)) {
        SafeRelease(writer);
        MFShutdown();
        return fail("format vidéo refusé par l'encodeur", hr);
    }
    impl = new Impl;
    impl->writer = writer;
    impl->stream = stream;
    impl->w = width;
    impl->h = height;
    impl->fps = std::max(fps, 1u);
    impl->bgra.resize((size_t)width * height * 4);
    return true;
}

bool VideoWriter::Write(const u8* rgba, u32 width, u32 height) {
    if (!impl || !rgba) return false;
    const u32 W = impl->w, H = impl->h;
    for (u32 y = 0; y < H; ++y) {
        u8* d = &impl->bgra[(size_t)y * W * 4];
        if (y >= height) {
            memset(d, 0, (size_t)W * 4);
            continue;
        }
        const u8* s = rgba + (size_t)y * width * 4;
        for (u32 x = 0; x < W; ++x, d += 4) {
            if (x >= width) { d[0] = d[1] = d[2] = 0; d[3] = 255; continue; }
            d[0] = s[x * 4 + 2];
            d[1] = s[x * 4 + 1];
            d[2] = s[x * 4];
            d[3] = 255;
        }
    }
    IMFMediaBuffer* buf = nullptr;
    const DWORD bytes = W * H * 4;
    if (FAILED(MFCreateMemoryBuffer(bytes, &buf))) return false;
    BYTE* p = nullptr;
    buf->Lock(&p, nullptr, nullptr);
    memcpy(p, impl->bgra.data(), bytes);
    buf->Unlock();
    buf->SetCurrentLength(bytes);
    IMFSample* sample = nullptr;
    MFCreateSample(&sample);
    sample->AddBuffer(buf);
    const LONGLONG dur = 10000000LL / impl->fps;
    sample->SetSampleTime(impl->frame * dur);
    sample->SetSampleDuration(dur);
    HRESULT hr = impl->writer->WriteSample(impl->stream, sample);
    SafeRelease(sample);
    SafeRelease(buf);
    ++impl->frame;
    return SUCCEEDED(hr);
}

bool VideoWriter::Close() {
    if (!impl) return true;
    HRESULT hr = impl->writer->Finalize();
    SafeRelease(impl->writer);
    delete impl;
    impl = nullptr;
    MFShutdown();
    return SUCCEEDED(hr);
}

}  // namespace sx

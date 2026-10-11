#include "io/RawLoad.h"
#include "io/WicLoad.h"

#include <cmath>
#include <cstring>
#include <stb_image.h>
#include <cstdio>
#include <memory>

#ifdef SX_LIBRAW
#include <windows.h>
#include <libraw/libraw.h>
#endif

namespace sx {

std::string RawInfo::Summary() const {
    std::string s = make.empty() ? model : (model.find(make) == 0 ? model : make + " " + model);
    if (!lens.empty()) s += (s.empty() ? "" : ", ") + lens;
    char buf[128];
    if (focal > 0) {
        snprintf(buf, sizeof(buf), ", %.0f mm", focal);
        s += buf;
    }
    if (aperture > 0) {
        snprintf(buf, sizeof(buf), " f/%.1f", aperture);
        s += buf;
    }
    if (shutter > 0) {
        if (shutter < 1) snprintf(buf, sizeof(buf), ", 1/%.0f s", 1.0 / shutter);
        else snprintf(buf, sizeof(buf), ", %.1f s", shutter);
        s += buf;
    }
    if (iso > 0) {
        snprintf(buf, sizeof(buf), ", ISO %.0f", iso);
        s += buf;
    }
    return s;
}

#ifdef SX_LIBRAW

namespace {

std::wstring WidePath(const std::string& utf8) {
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring w(n > 0 ? n - 1 : 0, L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, w.data(), n);
    return w;
}

void FillInfo(const LibRaw& rp, RawInfo& info) {
    const auto& d = rp.imgdata;
    info.make = d.idata.normalized_make[0] ? d.idata.normalized_make : d.idata.make;
    info.model = d.idata.normalized_model[0] ? d.idata.normalized_model : d.idata.model;
    info.lensMake = d.lens.LensMake;
    info.lens = d.lens.Lens[0] ? d.lens.Lens : d.lens.makernotes.Lens;
    info.focal = d.other.focal_len;
    info.aperture = d.other.aperture;
    info.iso = d.other.iso_speed;
    info.shutter = d.other.shutter;
    info.focal35 = d.lens.FocalLengthIn35mmFormat ? (float)d.lens.FocalLengthIn35mmFormat : d.lens.makernotes.FocalLengthIn35mmFormat;
    info.cropFactor = info.focal > 0 && info.focal35 > 0 ? info.focal35 / info.focal : 0;
    info.width = d.sizes.iwidth ? d.sizes.iwidth : d.sizes.width;
    info.height = d.sizes.iheight ? d.sizes.iheight : d.sizes.height;
    if (d.sizes.flip & 4) std::swap(info.width, info.height);
}

}  // namespace

bool HaveLibRaw() { return true; }

bool ReadRawInfo(const std::string& path, RawInfo& info) {
    auto rp = std::make_unique<LibRaw>();
    if (rp->open_file(WidePath(path).c_str()) != LIBRAW_SUCCESS) return false;
    FillInfo(*rp, info);
    return true;
}

bool LoadRawThumb(const std::string& path, std::vector<u8>& rgba, int& w, int& h, RawInfo* info) {
    auto rp = std::make_unique<LibRaw>();
    if (rp->open_file(WidePath(path).c_str()) != LIBRAW_SUCCESS) return false;
    if (info) FillInfo(*rp, *info);
    if (rp->unpack_thumb() != LIBRAW_SUCCESS) return false;
    int err = 0;
    libraw_processed_image_t* img = rp->dcraw_make_mem_thumb(&err);
    if (!img) return false;
    std::vector<u8> px;
    int iw = 0, ih = 0;
    if (img->type == LIBRAW_IMAGE_JPEG) {
        int comp = 0;
        if (stbi_uc* p = stbi_load_from_memory(img->data, (int)img->data_size, &iw, &ih, &comp, 4)) {
            px.assign(p, p + (size_t)iw * ih * 4);
            stbi_image_free(p);
        }
    } else if (img->type == LIBRAW_IMAGE_BITMAP && img->bits == 8 && (img->colors == 3 || img->colors == 1)) {
        iw = img->width;
        ih = img->height;
        px.resize((size_t)iw * ih * 4);
        for (size_t i = 0, n = (size_t)iw * ih; i < n; ++i) {
            const u8* s = img->data + i * img->colors;
            px[i * 4] = s[0];
            px[i * 4 + 1] = s[img->colors > 1 ? 1 : 0];
            px[i * 4 + 2] = s[img->colors > 1 ? 2 : 0];
            px[i * 4 + 3] = 255;
        }
    }
    LibRaw::dcraw_clear_mem(img);
    if (px.empty()) return false;
    // Camera orientation (LibRaw flip: 3 = 180, 5 = 90 counter-clockwise, 6 = 90 clockwise).
    const int flip = rp->imgdata.sizes.flip;
    if (flip == 3 || flip == 5 || flip == 6) {
        const bool swap = flip != 3;
        const int ow = swap ? ih : iw, oh = swap ? iw : ih;
        rgba.resize((size_t)ow * oh * 4);
        for (int y = 0; y < oh; ++y)
            for (int x = 0; x < ow; ++x) {
                int sx, sy;
                if (flip == 3) { sx = iw - 1 - x; sy = ih - 1 - y; }
                else if (flip == 6) { sx = y; sy = ih - 1 - x; }
                else { sx = iw - 1 - y; sy = x; }
                memcpy(&rgba[((size_t)y * ow + x) * 4], &px[((size_t)sy * iw + sx) * 4], 4);
            }
        w = ow;
        h = oh;
    } else {
        rgba = std::move(px);
        w = iw;
        h = ih;
    }
    return true;
}

bool LoadRawFloat(const std::string& path, std::vector<float>& rgba, int& w, int& h, RawInfo* info, std::string* error,
                  bool halfSize) {
    auto rp = std::make_unique<LibRaw>();  // large object (hundreds of KB): heap
    auto fail = [&](int code) {
        if (error) *error = libraw_strerror(code);
        return false;
    };
    int r = rp->open_file(WidePath(path).c_str());
    if (r != LIBRAW_SUCCESS) return fail(r);
    auto& p = rp->imgdata.params;
    p.output_bps = 16;
    p.gamm[0] = 1.0;  // linear
    p.gamm[1] = 1.0;
    p.no_auto_bright = 1;
    p.use_camera_wb = 1;
    p.use_camera_matrix = 1;
    p.output_color = 1;  // sRGB primaries
    p.highlight = 0;     // clip
    p.half_size = halfSize ? 1 : 0;
    p.user_qual = 3;     // AHD
    if ((r = rp->unpack()) != LIBRAW_SUCCESS) return fail(r);
    if ((r = rp->dcraw_process()) != LIBRAW_SUCCESS) return fail(r);
    libraw_processed_image_t* img = rp->dcraw_make_mem_image(&r);
    if (!img) return fail(r);
    if (img->type != LIBRAW_IMAGE_BITMAP || img->bits != 16 || (img->colors != 3 && img->colors != 1)) {
        LibRaw::dcraw_clear_mem(img);
        if (error) *error = "sortie LibRaw inattendue";
        return false;
    }
    w = img->width;
    h = img->height;
    rgba.resize((size_t)w * h * 4);
    const u16* src = (const u16*)img->data;
    const int nc = img->colors;
    for (size_t i = 0, n = (size_t)w * h; i < n; ++i) {
        const u16* s = src + i * nc;
        float* d = &rgba[i * 4];
        d[0] = s[0] / 65535.0f;
        d[1] = s[nc > 1 ? 1 : 0] / 65535.0f;
        d[2] = s[nc > 1 ? 2 : 0] / 65535.0f;
        d[3] = 1.0f;
    }
    LibRaw::dcraw_clear_mem(img);
    if (info) FillInfo(*rp, *info), info->width = w, info->height = h;
    return true;
}

#else

bool HaveLibRaw() { return false; }
bool ReadRawInfo(const std::string&, RawInfo&) { return false; }
bool LoadRawThumb(const std::string&, std::vector<u8>&, int&, int&, RawInfo*) { return false; }
bool LoadRawFloat(const std::string&, std::vector<float>&, int&, int&, RawInfo*, std::string* error, bool) {
    if (error) *error = "LibRaw absent (external/LibRaw)";
    return false;
}

#endif

bool LoadPhotoFloat(const std::string& path, std::vector<float>& rgba, int& w, int& h, RawInfo* info, std::string* error) {
    std::string ext;
    size_t dot = path.find_last_of('.');
    if (dot != std::string::npos) ext = path.substr(dot);
    for (auto& c : ext) c = (char)tolower((unsigned char)c);
    std::string rawErr;
    if (HaveLibRaw() && IsRawExtension(ext) && !getenv("SX_NOLIBRAW")) {
        if (LoadRawFloat(path, rgba, w, h, info, &rawErr)) return true;
    }
    if (LoadWicFloat(path, rgba, w, h, error)) {
        if (info && HaveLibRaw()) ReadRawInfo(path, *info);
        return true;
    }
    if (error && !rawErr.empty()) *error = "LibRaw : " + rawErr + " ; WIC : " + *error;
    return false;
}

}  // namespace sx

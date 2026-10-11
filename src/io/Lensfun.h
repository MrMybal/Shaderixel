#pragma once
// Lens profiles of the lensfun database (https://lensfun.github.io, CC BY-SA 3.0), read from its XML
// files (downloaded by the user: no library needed). Cameras (crop factor), lenses (calibrations of
// distortion, transverse chromatic aberration, vignetting) and the profile interpolated for a shot
// (focal length, aperture, distance): the parameters of the "Correction d'objectif" effect.
#include "core/Common.h"

#include <string>
#include <vector>

namespace sx {

struct LensfunDistortion { float focal = 0; int model = 0; float a = 0, b = 0, c = 0; };  // model 1 poly3 (k1 = a), 2 poly5 (k1, k2), 3 ptlens
struct LensfunTca { float focal = 0; int model = 0; float vr = 1, vb = 1, cr = 0, cb = 0, br = 0, bb = 0; };  // 1 linear (kr, kb), 2 poly3
struct LensfunVignetting { float focal = 0, aperture = 0, distance = 0; float k1 = 0, k2 = 0, k3 = 0; };

struct LensfunLens {
    std::string maker, model, mounts;
    float crop = 1;
    std::vector<LensfunDistortion> distortion;
    std::vector<LensfunTca> tca;
    std::vector<LensfunVignetting> vignetting;
    float minFocal = 0, maxFocal = 0;
    std::string Name() const { return model.find(maker) == 0 || maker.empty() ? model : maker + " " + model; }
};

struct LensfunCamera {
    std::string maker, model, mount;
    float crop = 1;
};

// Effect parameters for one shot.
struct LensProfile {
    int distModel = 0;
    float a = 0, b = 0, c = 0;
    int tcaModel = 0;
    float vr = 1, vb = 1, cr = 0, cb = 0, br = 0, bb = 0;
    bool vignetting = false;
    float k1 = 0, k2 = 0, k3 = 0;
    float cropRatio = 1;  // calibration crop / camera crop
    std::string lens;
    float focal = 0;
};

class LensfunDb {
public:
    // Every *.xml with a <lensdatabase> under dir (a few levels deep: data/db, share/lensfun/version_1...).
    bool Load(const std::string& dir, std::string* error = nullptr);
    bool Loaded() const { return !lenses.empty(); }
    const std::string& Dir() const { return dir; }
    std::vector<LensfunLens> lenses;
    std::vector<LensfunCamera> cameras;
    const LensfunCamera* FindCamera(const std::string& maker, const std::string& model) const;
    // Best match of a lens name (EXIF lens model), nullptr if nothing close.
    const LensfunLens* FindLens(const std::string& name, const std::string& maker = "") const;
    // Interpolated profile (focal mm, aperture f-number, distance m), cameraCrop 0: the lens's own crop.
    static LensProfile Profile(const LensfunLens& lens, float focal, float aperture, float distance, float cameraCrop);

private:
    std::string dir;
};

// The folder of the database: the setting, else the usual places (%LOCALAPPDATA%\Shaderixel\lensfun,
// next to the exe, darktable / lensfun installations). Empty if none.
std::string FindLensfunDir(const std::string& setting);
LensfunDb& Lensfun();  // loaded on first use from FindLensfunDir

}  // namespace sx

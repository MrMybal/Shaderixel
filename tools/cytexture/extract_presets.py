"""Extracts CyTexture's layer presets (read-only) into tools/cytexture/presets.json.

CyTexture defines its presets in CyTextureMaterialPresetGenerator.cpp: category,
name, description, Custom-node HLSL, texture inputs, scalar and vector
parameters. The part of that file that builds the preset list uses no Unreal
API, so it is copied (never modified in place) into a temporary C++ file with a
few stand-in types and compiled with MSVC; the program prints the list as JSON.

    python tools/cytexture/extract_presets.py [path to CyTextureMaterialPresetGenerator.cpp]
"""
import json
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_SRC = r'<CyTexture_plugin>\Plugins\CyTexture\Source\CyTexture\Private\CyTextureMaterialPresetGenerator.cpp'

SHIM = r'''
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <vector>
#define TEXT(x) x
#define NAME_None FName()
using TCHAR = char;
using uint8 = unsigned char;
using int32 = int;
using uint32 = unsigned int;
struct FString {
    std::string s;
    FString() {}
    FString(const char* c) : s(c) {}
    FString(std::string x) : s(std::move(x)) {}
    const char* operator*() const { return s.c_str(); }
    FString operator+(const FString& o) const { return s + o.s; }
    FString operator+(const char* o) const { return s + o; }
    friend FString operator+(const char* a, const FString& b) { return std::string(a) + b.s; }
    FString& operator+=(const FString& o) { s += o.s; return *this; }
    bool operator==(const char* o) const { return s == o; }
    bool Contains(const char* n) const { return s.find(n) != std::string::npos; }
    template <class... A> static FString Printf(const char* f, A... a) {
        static char buf[1 << 20];
        snprintf(buf, sizeof(buf), f, a...);
        return FString(buf);
    }
    template <class C> static FString Join(const C& parts, const char* sep) {
        std::string r;
        for (size_t i = 0; i < parts.size(); ++i) { if (i) r += sep; r += parts[i].s; }
        return r;
    }
};
struct FName {
    std::string s;
    FName(const char* c = "") : s(c) {}
    FName(const FString& f) : s(f.s) {}
    bool operator==(const char* o) const { return s == o; }
};
template <class T> struct TArray : std::vector<T> {
    using std::vector<T>::vector;
    TArray() {}
    TArray(std::initializer_list<T> l) : std::vector<T>(l) {}
    void Add(const T& v) { this->push_back(v); }
    void Append(const TArray& o) { this->insert(this->end(), o.begin(), o.end()); }
    int Num() const { return (int)this->size(); }
};
struct FLinearColor {
    float R = 0, G = 0, B = 0, A = 1;
    FLinearColor() {}
    FLinearColor(float r, float g, float b, float a = 1) : R(r), G(g), B(b), A(a) {}
    static const FLinearColor White, Black, Transparent, Red, Green, Blue, Yellow, Gray;
};
const FLinearColor FLinearColor::White(1, 1, 1, 1), FLinearColor::Black(0, 0, 0, 1), FLinearColor::Transparent(0, 0, 0, 0),
    FLinearColor::Red(1, 0, 0, 1), FLinearColor::Green(0, 1, 0, 1), FLinearColor::Blue(0, 0, 1, 1), FLinearColor::Yellow(1, 1, 0, 1),
    FLinearColor::Gray(0.5f, 0.5f, 0.5f, 1);
'''

MAIN = r'''
static void Str(const std::string& s) {
    putchar('"');
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { putchar('\\'); putchar(c); }
        else if (c == '\n') fputs("\\n", stdout);
        else if (c == '\r') fputs("\\r", stdout);
        else if (c == '\t') fputs("\\t", stdout);
        else if (c < 0x20) printf("\\u%04x", c);
        else putchar(c);
    }
    putchar('"');
}
int main() {
    TArray<FMaterialPreset> all;
    %CALLS%
    const char* kinds[] = {"Draw", "Effect", "Blend", "Mask"};
    printf("[\n");
    for (size_t i = 0; i < all.size(); ++i) {
        const FMaterialPreset& p = all[i];
        printf("{\"category\":"); Str(p.Category.s);
        printf(",\"name\":"); Str(p.Name.s);
        printf(",\"description\":"); Str(p.Description.s);
        printf(",\"kind\":\"%s\"", kinds[(int)p.Kind]);
        printf(",\"group\":"); Str(ResolvePresetGroup(p).s);
        printf(",\"code\":"); Str(p.Code.s);
        printf(",\"textures\":[");
        for (size_t k = 0; k < p.Textures.size(); ++k) { if (k) putchar(','); Str(p.Textures[k].s); }
        printf("],\"scalars\":[");
        for (size_t k = 0; k < p.Scalars.size(); ++k) {
            auto& s = p.Scalars[k];
            if (k) putchar(',');
            printf("{\"name\":"); Str(s.Name.s);
            printf(",\"default\":%.9g,\"min\":%.9g,\"max\":%.9g,\"fine\":%.9g,\"normal\":%.9g,\"coarse\":%.9g,\"widget\":", s.Default, s.Minimum, s.Maximum, s.Fine, s.Normal, s.Coarse);
            Str(s.EditorWidget.s);
            printf(",\"hidden\":%s}", s.bHideInLiveEditor ? "true" : "false");
        }
        printf("],\"vectors\":[");
        for (size_t k = 0; k < p.Vectors.size(); ++k) {
            auto& v = p.Vectors[k];
            if (k) putchar(',');
            printf("{\"name\":"); Str(v.Name.s);
            printf(",\"default\":[%.9g,%.9g,%.9g,%.9g],\"widget\":", v.Default.R, v.Default.G, v.Default.B, v.Default.A);
            Str(v.EditorWidget.s);
            printf(",\"hidden\":%s}", v.bHideInLiveEditor ? "true" : "false");
        }
        printf("]}%s\n", i + 1 < all.size() ? "," : "");
    }
    printf("]\n");
}
'''


def find_vcvars():
    vswhere = os.path.expandvars(r'%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe')
    path = subprocess.check_output([vswhere, '-latest', '-products', '*', '-property', 'installationPath'], text=True).strip()
    return os.path.join(path, r'VC\Auxiliary\Build\vcvars64.bat')


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SRC
    text = open(src, encoding='utf-8-sig').read()
    lines = text.split('\n')
    start = next(i for i, l in enumerate(lines) if l.strip() == 'namespace')
    end = next(i for i, l in enumerate(lines) if 'UClass* ContractClass' in l)
    body = '\n'.join(lines[start:end])
    # Preset group helpers (pure string logic), kept from the same file.
    g0 = next(i for i, l in enumerate(lines) if 'bool PresetNameIn(' in l)
    g1 = next(i for i, l in enumerate(lines) if 'void ResolvePresetMetadata(' in l)
    groups = '\n'.join(lines[g0:g1])
    adders = re.findall(r'void (Add\w+Presets)\(TArray<FMaterialPreset>& Out\)', body)
    calls = '\n    '.join(f'{a}(all);' for a in adders)
    cpp = SHIM + body + '\n' + groups + '\n}\n' + MAIN.replace('%CALLS%', calls)
    work = tempfile.mkdtemp(prefix='cypresets_')
    cpp_path = os.path.join(work, 'presets.cpp')
    open(cpp_path, 'w', encoding='utf-8').write(cpp)
    exe = os.path.join(work, 'presets.exe')
    cmd = f'call "{find_vcvars()}" >nul && cl /nologo /std:c++20 /EHsc /O1 /utf-8 /w "{cpp_path}" /Fe"{exe}" /Fo"{work}\\\\"'
    r = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout[-6000:], r.stderr[-3000:])
        sys.exit('compilation failed (see above)')
    out = subprocess.run([exe], capture_output=True)
    presets = json.loads(out.stdout.decode('utf-8'))
    dst = os.path.join(HERE, 'presets.json')
    json.dump(presets, open(dst, 'w', encoding='utf-8'), indent=1, ensure_ascii=False)
    kinds = {}
    for p in presets:
        kinds[p['category']] = kinds.get(p['category'], 0) + 1
    print(f'{len(presets)} presets -> {dst}: {kinds} (adders: {", ".join(adders)})')


if __name__ == '__main__':
    main()

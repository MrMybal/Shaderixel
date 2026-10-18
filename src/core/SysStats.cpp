#include "core/SysStats.h"
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cwctype>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace sx::sys {
namespace {

// Wildcard counter values: instance name -> value (empty when the counter is not available).
std::vector<std::pair<std::wstring, double>> ReadArray(PDH_HCOUNTER c, DWORD format) {
    std::vector<std::pair<std::wstring, double>> out;
    if (!c) return out;
    DWORD bytes = 0, count = 0;
    if (PdhGetFormattedCounterArrayW(c, format, &bytes, &count, nullptr) != PDH_MORE_DATA) return out;
    std::vector<BYTE> buf(bytes);
    auto* items = (PDH_FMT_COUNTERVALUE_ITEM_W*)buf.data();
    if (PdhGetFormattedCounterArrayW(c, format, &bytes, &count, items) != ERROR_SUCCESS) return out;
    out.reserve(count);
    for (DWORD i = 0; i < count; ++i) {
        if (items[i].FmtValue.CStatus != PDH_CSTATUS_VALID_DATA && items[i].FmtValue.CStatus != PDH_CSTATUS_NEW_DATA) continue;
        double v = format & PDH_FMT_LARGE ? (double)items[i].FmtValue.largeValue : items[i].FmtValue.doubleValue;
        out.emplace_back(items[i].szName, v);
    }
    return out;
}

bool ContainsNoCase(const std::wstring& s, const std::wstring& part) {
    if (part.empty()) return true;
    auto it = std::search(s.begin(), s.end(), part.begin(), part.end(),
                          [](wchar_t a, wchar_t b) { return std::towlower(a) == std::towlower(b); });
    return it != s.end();
}

struct Monitor {
    std::mutex m;
    std::condition_variable cv;
    std::thread worker;
    bool stop = false;
    u64 luid = 0;
    Load load;

    ~Monitor() {
        {
            std::lock_guard<std::mutex> lk(m);
            stop = true;
        }
        cv.notify_all();
        if (worker.joinable()) worker.join();
    }

    void Start(u64 adapter) {
        luid = adapter;
        worker = std::thread([this] { Run(); });
    }

    void Run() {
        // English counter names: the localized ones differ on a French Windows.
        PDH_HQUERY q = nullptr;
        PDH_HCOUNTER engines = nullptr, memory = nullptr;
        if (PdhOpenQueryW(nullptr, 0, &q) == ERROR_SUCCESS) {
            if (PdhAddEnglishCounterW(q, L"\\GPU Engine(*)\\Utilization Percentage", 0, &engines) != ERROR_SUCCESS) engines = nullptr;
            if (PdhAddEnglishCounterW(q, L"\\GPU Adapter Memory(*)\\Dedicated Usage", 0, &memory) != ERROR_SUCCESS) memory = nullptr;
            PdhCollectQueryData(q);
        }
        // Instances are named "pid_N_luid_0xHIGH_0xLOW_phys_0_eng_3_engtype_3D" / "luid_0xHIGH_0xLOW_phys_0".
        std::wstring tag;
        if (luid) {
            wchar_t b[64];
            swprintf(b, 64, L"luid_0x%08X_0x%08X", (unsigned)(luid >> 32), (unsigned)(luid & 0xffffffffu));
            tag = b;
        }
        FILETIME pi, pk, pu;
        GetSystemTimes(&pi, &pk, &pu);
        auto u64of = [](const FILETIME& f) { return ((u64)f.dwHighDateTime << 32) | f.dwLowDateTime; };

        std::unique_lock<std::mutex> lk(m);
        while (!cv.wait_for(lk, std::chrono::milliseconds(1000), [this] { return stop; })) {
            lk.unlock();
            Load l;
            // Processor: busy share of the time since the last sample (kernel time includes idle).
            FILETIME ci, ck, cu;
            if (GetSystemTimes(&ci, &ck, &cu)) {
                u64 idle = u64of(ci) - u64of(pi), total = (u64of(ck) - u64of(pk)) + (u64of(cu) - u64of(pu));
                if (total) l.cpu = std::clamp(1.0f - (float)idle / (float)total, 0.0f, 1.0f);
                pi = ci, pk = ck, pu = cu;
            }
            if (q && PdhCollectQueryData(q) == ERROR_SUCCESS) {
                // Graphics card: the processes add up per engine, the busiest engine is the load.
                auto e = ReadArray(engines, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100);
                if (!e.empty()) {
                    std::map<std::wstring, double> perEngine;
                    for (auto& [name, v] : e) {
                        if (!ContainsNoCase(name, tag)) continue;
                        size_t at = name.find(L"luid_");
                        perEngine[at == std::wstring::npos ? name : name.substr(at)] += v;
                    }
                    double busiest = 0;
                    for (auto& [k, v] : perEngine) busiest = std::max(busiest, v);
                    l.gpu = std::clamp((float)(busiest / 100.0), 0.0f, 1.0f);
                }
                auto mem = ReadArray(memory, PDH_FMT_LARGE);
                for (auto& [name, v] : mem)
                    if (ContainsNoCase(name, tag)) {
                        l.vramUsedAll += (u64)v;
                        l.vramKnown = true;
                    }
            }
            lk.lock();
            load = l;
        }
        lk.unlock();
        if (q) PdhCloseQuery(q);
    }
};

Monitor& Mon() {
    static Monitor mon;
    return mon;
}

}  // namespace

void WatchAdapter(u64 luid) {
    Monitor& mon = Mon();
    if (!mon.worker.joinable()) mon.Start(luid);
}

Load CurrentLoad() {
    Monitor& mon = Mon();
    std::lock_guard<std::mutex> lk(mon.m);
    return mon.load;
}

}  // namespace sx::sys

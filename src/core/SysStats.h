#pragma once
#include "core/Common.h"

// Machine load read from the Windows performance counters on a background thread (about once a second):
// processor, graphics card (busiest engine, like the Task Manager) and its dedicated memory used by all programs.
namespace sx::sys {

struct Load {
    float cpu = -1;        // 0..1, -1 until known
    float gpu = -1;        // 0..1, -1 when the counters are not available
    u64 vramUsedAll = 0;   // dedicated memory of the adapter used by every process (0 when unknown)
    bool vramKnown = false;
};

// Starts the sampling for the adapter with this LUID (0: every adapter). Cheap to call every frame.
void WatchAdapter(u64 luid);
Load CurrentLoad();

}  // namespace sx::sys

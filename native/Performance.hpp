#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <algorithm>

namespace Wardrobe {
extern bool logging;
enum class Operation { Attach, Catalog, CatalogIndex, AssetLoad, Refresh, Menu, Open, Count };
struct OperationTiming { uint64_t count{},micros{},maximum{}; };
inline std::array<OperationTiming,static_cast<size_t>(Operation::Count)> operationTimings;
// Optional diagnostics only. Soft scheduling deadlines remain independent.
struct MeasureOperation {
    Operation operation;bool enabled;
    std::chrono::steady_clock::time_point start;
    explicit MeasureOperation(Operation value):operation(value),enabled(logging),start(enabled?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{}){}
    ~MeasureOperation(){if(enabled){auto elapsed=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count());auto& entry=operationTimings[static_cast<size_t>(operation)];++entry.count;entry.micros+=elapsed;entry.maximum=std::max(entry.maximum,elapsed);}}
};
}

#pragma once
#include <cstdint>

#include "generated/kcd_symbols.h"

// Resolves the addresses in generated/kcd_symbols.h (from kcd-re: `kre db export`) inside WHGame.dll.
// Functions are found by signature; data symbols (no signature) use their RVA only on the known build.
namespace kc::sym {

using kcd::sym::Id;

struct Status {
    uintptr_t module_base = 0;
    uint32_t module_timestamp = 0;
    bool known_build = false;  // WHGame.dll PE timestamp matches the build the symbols were taken from
    int resolved = 0, by_pattern = 0, by_rva = 0, missing = 0;
};

// Init thread, once WHGame.dll is loaded. Logs every symbol it could not resolve.
void resolve_all();
const Status& status();
// 0 when unresolved.
uintptr_t addr(Id id);
template <typename T>
T* ptr(Id id) { return reinterpret_cast<T*>(addr(id)); }

}  // namespace kc::sym

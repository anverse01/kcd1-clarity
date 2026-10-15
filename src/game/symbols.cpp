#include "game/symbols.h"

#include <windows.h>

#include <cstring>
#include <iterator>
#include <vector>

#include "core/log.h"

namespace kc::sym {

using kcd::sym::kSymbols;
static_assert(std::size(kSymbols) == static_cast<size_t>(Id::Count));

static Status g_status;
static uintptr_t g_addr[static_cast<size_t>(Id::Count)];

const Status& status() { return g_status; }
uintptr_t addr(Id id) { return g_addr[static_cast<size_t>(id)]; }

struct Pattern {
    std::vector<uint8_t> bytes;
    std::vector<uint8_t> mask;  // 1 = must match
};

static Pattern parse(const char* s) {
    Pattern p;
    while (*s) {
        while (*s == ' ')
            ++s;
        if (!*s)
            break;
        if (*s == '?') {
            p.bytes.push_back(0);
            p.mask.push_back(0);
            while (*s == '?')
                ++s;
        } else {
            p.bytes.push_back(static_cast<uint8_t>(strtoul(s, const_cast<char**>(&s), 16)));
            p.mask.push_back(1);
        }
    }
    return p;
}

// All matches of p in [begin, end), up to `limit`.
static std::vector<const uint8_t*> scan(const uint8_t* begin, const uint8_t* end, const Pattern& p, size_t limit) {
    std::vector<const uint8_t*> hits;
    const size_t n = p.bytes.size();
    if (n == 0 || !p.mask[0] || static_cast<size_t>(end - begin) < n)
        return hits;
    for (const uint8_t* at = begin; at + n <= end;) {
        at = static_cast<const uint8_t*>(memchr(at, p.bytes[0], static_cast<size_t>(end - at - n + 1)));
        if (!at)
            break;
        size_t i = 1;
        while (i < n && (!p.mask[i] || at[i] == p.bytes[i]))
            ++i;
        if (i == n) {
            hits.push_back(at);
            if (hits.size() >= limit)
                break;
        }
        ++at;
    }
    return hits;
}

void resolve_all() {
    auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"WHGame.dll"));
    if (!base) {
        logf("symbols: WHGame.dll not loaded");
        return;
    }
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    g_status.module_base = base;
    g_status.module_timestamp = nt->FileHeader.TimeDateStamp;
    g_status.known_build = g_status.module_timestamp == kcd::sym::kTimestamp_WHGame;
    logf("symbols: WHGame.dll at 0x%llx, timestamp 0x%08x (%s)", (unsigned long long)base,
         g_status.module_timestamp, g_status.known_build ? "known build" : "UNKNOWN build");

    const uint8_t* text = nullptr;
    size_t text_size = 0;
    const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec)
        if (memcmp(sec->Name, ".text", 6) == 0) {
            text = reinterpret_cast<const uint8_t*>(base + sec->VirtualAddress);
            text_size = sec->Misc.VirtualSize;
        }

    for (size_t i = 0; i < std::size(kSymbols); ++i) {
        const auto& s = kSymbols[i];
        uintptr_t a = 0;
        const char* how = "missing";
        if (s.pattern && text) {
            auto hits = scan(text, text + text_size, parse(s.pattern), 2);
            if (hits.size() == 1) {
                const uint8_t* at = hits[0] + s.pattern_offset;
                if (s.resolve == 1)  // E8 rel32: the symbol is the call target
                    at = at + 5 + *reinterpret_cast<const int32_t*>(at + 1);
                a = reinterpret_cast<uintptr_t>(at);
                how = "pattern";
                ++g_status.by_pattern;
            } else {
                logf("symbols: %s pattern matched %zu times", s.name, hits.size());
            }
        }
        if (!a && g_status.known_build) {
            a = base + s.rva;
            how = "rva";
            ++g_status.by_rva;
        }
        if (a && g_status.known_build && a != base + s.rva)
            logf("symbols: %s found at rva 0x%llx, db says 0x%x", s.name, (unsigned long long)(a - base), s.rva);
        g_addr[i] = a;
        if (a)
            ++g_status.resolved;
        else
            ++g_status.missing;
        if (!a)
            logf("symbols: %s not resolved", s.name);
        else if (!g_status.known_build)
            logf("symbols: %s -> rva 0x%llx (%s)", s.name, (unsigned long long)(a - base), how);
    }
    logf("symbols: %d resolved (%d by pattern, %d by rva), %d missing", g_status.resolved, g_status.by_pattern,
         g_status.by_rva, g_status.missing);
}

}  // namespace kc::sym

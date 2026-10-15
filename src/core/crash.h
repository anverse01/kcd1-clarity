#pragma once

namespace kc {

// Logs fatal exceptions (code, module+offset, unwound stack) to the mod log; the first few only.
void install_crash_logger();

}  // namespace kc

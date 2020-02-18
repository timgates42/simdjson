#include "simdjson/architecture.h"
#include "simdjson/isadetection.h"
#include <string.h>

namespace simdjson {

architecture find_best_supported_architecture() {
  constexpr uint32_t haswell_flags =
      instruction_set::AVX2 | instruction_set::PCLMULQDQ |
      instruction_set::BMI1 | instruction_set::BMI2;
  constexpr uint32_t westmere_flags =
      instruction_set::SSE42 | instruction_set::PCLMULQDQ;

  uint32_t supports = detect_supported_architectures();
  // Order from best to worst (within architecture)
  if ((haswell_flags & supports) == haswell_flags)
    return architecture::HASWELL;
  if ((westmere_flags & supports) == westmere_flags)
    return architecture::WESTMERE;
  if (supports & instruction_set::NEON)
    return architecture::ARM64;

  return architecture::UNSUPPORTED;
}

architecture parse_architecture(char *arch) {
  if (!strcmp(arch, "HASWELL")) { return architecture::HASWELL; }
  if (!strcmp(arch, "WESTMERE")) { return architecture::WESTMERE; }
  if (!strcmp(arch, "ARM64")) { return architecture::ARM64; }
  return architecture::UNSUPPORTED;
}

} // namespace simdjson
#ifndef SIMDJSON_WESTMERE_STAGE2_BUILD_TAPE_H
#define SIMDJSON_WESTMERE_STAGE2_BUILD_TAPE_H

#include "simdjson/portability.h"

#ifdef IS_X86_64

#include "simdjson/document.h"
#include "simdjson/stage2_build_tape.h"
#include "westmere/stringparsing.h"
#include "westmere/numberparsing.h"

TARGET_WESTMERE
namespace simdjson::westmere {

#include "generic/stage2_build_tape.h"
#include "generic/stage2_streaming_build_tape.h"

} // namespace simdjson::westmere
UNTARGET_REGION

TARGET_WESTMERE
namespace simdjson::internal {

template<>
WARN_UNUSED error_code document_parser_implementation::for_architecture<architecture::WESTMERE>::stage2(const uint8_t *buf, size_t len, document::parser &parser) const noexcept {
  return westmere::stage2::unified_machine(buf, len, parser);
}

template<>
WARN_UNUSED error_code document_parser_implementation::for_architecture<architecture::WESTMERE>::stage2(const uint8_t *buf, size_t len, document::parser &parser, size_t &next_json) const noexcept {
  return westmere::stage2::unified_machine(buf, len, parser, next_json);
}

} // namespace simdjson
UNTARGET_REGION

#endif // IS_X86_64

#endif // SIMDJSON_WESTMERE_STAGE2_BUILD_TAPE_H

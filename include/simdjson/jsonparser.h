#ifndef SIMDJSON_JSONPARSER_H
#define SIMDJSON_JSONPARSER_H

#include "simdjson/document.h"
#include "simdjson/parsedjson.h"
#include "simdjson/jsonioutil.h"

namespace simdjson {

//
// C API (json_parse and build_parsed_json) declarations
//

inline int json_parse(const uint8_t *buf, size_t len, document::parser &parser, bool realloc_if_needed = true) noexcept {
  return parser.parse(buf, len, realloc_if_needed);
}
inline int json_parse(const char *buf, size_t len, document::parser &parser, bool realloc_if_needed = true) noexcept {
  return parser.parse(buf, len, realloc_if_needed);
}
inline int json_parse(const std::string &s, document::parser &parser, bool realloc_if_needed = true) noexcept {
  return parser.parse(s, realloc_if_needed);
}
inline int json_parse(const padded_string &s, document::parser &parser) noexcept {
  return parser.parse(s);
}

WARN_UNUSED static document::parser build_parsed_json(const uint8_t *buf, size_t len, bool realloc_if_needed = true) noexcept {
  document::parser parser;
  if (!parser.allocate_capacity(len)) {
    parser.valid = false;
    parser.error = MEMALLOC;
    return parser;
  }
  error_code code = parser.parse(buf, len, realloc_if_needed).error;
  parser.valid = code != SUCCESS;
  parser.error = code;
  return parser;
}
WARN_UNUSED inline document::parser build_parsed_json(const char *buf, size_t len, bool realloc_if_needed = true) noexcept {
  return build_parsed_json(reinterpret_cast<const uint8_t *>(buf), len, realloc_if_needed);
}
WARN_UNUSED inline document::parser build_parsed_json(const std::string &s, bool realloc_if_needed = true) noexcept {
  return build_parsed_json(s.data(), s.length(), realloc_if_needed);
}
WARN_UNUSED inline document::parser build_parsed_json(const padded_string &s) noexcept {
  return build_parsed_json(s.data(), s.length(), false);
}

// We do not want to allow implicit conversion from C string to std::string.
int json_parse(const char *buf, document::parser &parser) noexcept = delete;
document::parser build_parsed_json(const char *buf) noexcept = delete;

} // namespace simdjson

#endif

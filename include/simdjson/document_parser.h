#ifndef SIMDJSON_DOCUMENT_PARSER_H
#define SIMDJSON_DOCUMENT_PARSER_H

#include <atomic>
#include <cstring>
#include <memory>
#include "simdjson/common_defs.h"
#include "simdjson/simdjson.h"
#include "simdjson/document.h"
#include "simdjson/padded_string.h"
#include "simdjson/portability.h"

// Declare the internal implementation classes first so we can reference them in document::parser
namespace simdjson::internal {

class document_parser_implementation {
public:
  virtual architecture get_architecture() const noexcept = 0;
  WARN_UNUSED virtual error_code stage1(const uint8_t *buf, size_t len, document::parser &parser, bool streaming) const noexcept = 0;
  WARN_UNUSED virtual error_code stage2(const uint8_t *buf, size_t len, document::parser &parser) const noexcept = 0;
  WARN_UNUSED virtual error_code stage2(const uint8_t *buf, size_t len, document::parser &parser, size_t &next_json) const noexcept = 0;

  static document_parser_implementation* get_implementation(architecture arch);
  template<architecture A>
  class for_architecture;
  class find_best;
};

class document_parser_implementation::find_best final : public document_parser_implementation {
public:
  static find_best singleton;
  architecture get_architecture() const noexcept override {
    return get_best_implementation()->get_architecture();
  }
  WARN_UNUSED error_code stage1(const uint8_t *buf, size_t len, document::parser &parser, bool streaming) const noexcept final {
    return get_best_implementation()->stage1(buf, len, parser, streaming);
  }
  WARN_UNUSED error_code stage2(const uint8_t *buf, size_t len, document::parser &parser) const noexcept final {
    return get_best_implementation()->stage2(buf, len, parser);
  }
  WARN_UNUSED error_code stage2(const uint8_t *buf, size_t len, document::parser &parser, size_t &next_json) const noexcept final {
    return get_best_implementation()->stage2(buf, len, parser, next_json);
  }

private:
  document_parser_implementation *get_best_implementation() const;
};

template<architecture A>
class document_parser_implementation::for_architecture final : public document_parser_implementation {
public:
  static document_parser_implementation::for_architecture<A> singleton;
  architecture get_architecture() const noexcept final;
  WARN_UNUSED error_code stage1(const uint8_t *buf, size_t len, document::parser &parser, bool streaming) const noexcept final;
  WARN_UNUSED error_code stage2(const uint8_t *buf, size_t len, document::parser &parser) const noexcept final;
  WARN_UNUSED error_code stage2(const uint8_t *buf, size_t len, document::parser &parser, size_t &next_json) const noexcept final;
};

} // namespace simdjson::internal

namespace simdjson {

class document::parser {
public:
  //
  // Create a JSON parser with zero capacity. Call allocate_capacity() to initialize it.
  //
  parser()=default;
  ~parser()=default;

  // this is a move only class
  parser(document::parser &&p) = default;
  parser(const document::parser &p) = delete;
  parser &operator=(document::parser &&o) = default;
  parser &operator=(const document::parser &o) = delete;

  //
  // Parse a JSON document and return a reference to it.
  //
  // The JSON document still lives in the parser: this is the most efficient way to parse JSON
  // documents because it reuses the same buffers, but you *must* use the document before you
  // destroy the parser or call parse() again.
  //
  // Throws invalid_json if the JSON is invalid.
  //
  inline doc_ref_result parse(const uint8_t *buf, size_t len, bool realloc_if_needed = true);
  inline doc_ref_result parse(const char *buf, size_t len, bool realloc_if_needed = true);
  inline doc_ref_result parse(const std::string &s, bool realloc_if_needed = true);
  inline doc_ref_result parse(const padded_string &s);
  // We do not want to allow implicit conversion from C string to std::string.
  doc_ref_result parse(const char *buf, bool realloc_if_needed = true) = delete;

  //
  // Current capacity: the largest document this parser can support without reallocating.
  //
  size_t capacity() {
    return _capacity;
  }

  //
  // The maximum level of nested object and arrays supported by this parser.
  //
  size_t max_depth() {
    return _max_depth;
  }

  // if needed, allocate memory so that the object is able to process JSON
  // documents having up to capacity bytes and max_depth "depth"
  WARN_UNUSED bool allocate_capacity(size_t capacity, size_t max_depth = DEFAULT_MAX_DEPTH) {
    return set_capacity(capacity) && set_max_depth(max_depth);
  }

  // type aliases for backcompat
  using Iterator = document::iterator;
  using InvalidJSON = invalid_json;
  class doc_result;

  // Next location to write to in the tape
  uint32_t current_loc{0};

  // structural indices passed from stage 1 to stage 2
  uint32_t n_structural_indexes{0};
  std::unique_ptr<uint32_t[]> structural_indexes;

  // location and return address of each open { or [
  std::unique_ptr<uint32_t[]> containing_scope_offset;
#ifdef SIMDJSON_USE_COMPUTED_GOTO
  std::unique_ptr<void*[]> ret_address;
#else
  std::unique_ptr<char[]> ret_address;
#endif

  // Next place to write a string
  uint8_t *current_string_buf_loc;

  bool valid{false};
  error_code error{simdjson::UNINITIALIZED};

  // Document we're writing to
  document doc;

  // returns true if the document parsed was valid
  bool is_valid() const;

  // return an error code corresponding to the last parsing attempt, see
  // simdjson.h will return simdjson::UNITIALIZED if no parsing was attempted
  int get_error_code() const;

  // return the string equivalent of "get_error_code"
  std::string get_error_message() const;

  //
  // for backcompat with ParsedJson
  //

  // print the json to std::ostream (should be valid)
  // return false if the tape is likely wrong (e.g., you did not parse a valid
  // JSON).
  WARN_UNUSED
  bool print_json(std::ostream &os) const;
  WARN_UNUSED
  bool dump_raw_tape(std::ostream &os) const;

  // this should be called when parsing (right before writing the tapes)
  void init_stage2();

  really_inline error_code on_error(error_code new_error_code) {
    error = new_error_code;
    return new_error_code;
  }
  really_inline error_code on_success(error_code success_code) {
    error = success_code;
    valid = true;
    return success_code;
  }
  really_inline bool on_start_document(uint32_t depth) {
    containing_scope_offset[depth] = current_loc;
    write_tape(0, 'r');
    return true;
  }
  really_inline bool on_start_object(uint32_t depth) {
    containing_scope_offset[depth] = current_loc;
    write_tape(0, '{');
    return true;
  }
  really_inline bool on_start_array(uint32_t depth) {
    containing_scope_offset[depth] = current_loc;
    write_tape(0, '[');
    return true;
  }
  // TODO we're not checking this bool
  really_inline bool on_end_document(uint32_t depth) {
    // write our doc.tape location to the header scope
    // The root scope gets written *at* the previous location.
    annotate_previous_loc(containing_scope_offset[depth], current_loc);
    write_tape(containing_scope_offset[depth], 'r');
    return true;
  }
  really_inline bool on_end_object(uint32_t depth) {
    // write our doc.tape location to the header scope
    write_tape(containing_scope_offset[depth], '}');
    annotate_previous_loc(containing_scope_offset[depth], current_loc);
    return true;
  }
  really_inline bool on_end_array(uint32_t depth) {
    // write our doc.tape location to the header scope
    write_tape(containing_scope_offset[depth], ']');
    annotate_previous_loc(containing_scope_offset[depth], current_loc);
    return true;
  }

  really_inline bool on_true_atom() {
    write_tape(0, 't');
    return true;
  }
  really_inline bool on_false_atom() {
    write_tape(0, 'f');
    return true;
  }
  really_inline bool on_null_atom() {
    write_tape(0, 'n');
    return true;
  }

  really_inline uint8_t *on_start_string() {
    /* we advance the point, accounting for the fact that we have a NULL
      * termination         */
    write_tape(current_string_buf_loc - doc.string_buf.get(), '"');
    return current_string_buf_loc + sizeof(uint32_t);
  }

  really_inline bool on_end_string(uint8_t *dst) {
    uint32_t str_length = dst - (current_string_buf_loc + sizeof(uint32_t));
    // TODO check for overflow in case someone has a crazy string (>=4GB?)
    // But only add the overflow check when the document itself exceeds 4GB
    // Currently unneeded because we refuse to parse docs larger or equal to 4GB.
    memcpy(current_string_buf_loc, &str_length, sizeof(uint32_t));
    // NULL termination is still handy if you expect all your strings to
    // be NULL terminated? It comes at a small cost
    *dst = 0;
    current_string_buf_loc = dst + 1;
    return true;
  }

  really_inline bool on_number_s64(int64_t value) {
    write_tape(0, 'l');
    std::memcpy(&doc.tape[current_loc], &value, sizeof(value));
    ++current_loc;
    return true;
  }
  really_inline bool on_number_u64(uint64_t value) {
    write_tape(0, 'u');
    doc.tape[current_loc++] = value;
    return true;
  }
  really_inline bool on_number_double(double value) {
    write_tape(0, 'd');
    static_assert(sizeof(value) == sizeof(doc.tape[current_loc]), "mismatch size");
    memcpy(&doc.tape[current_loc++], &value, sizeof(double));
    // doc.tape[doc.current_loc++] = *((uint64_t *)&d);
    return true;
  }

  //
  // Called before a parse is initiated.
  //
  // - Returns CAPACITY if the document is too large
  // - Returns MEMALLOC if we needed to allocate memory and could not
  //
  WARN_UNUSED error_code init_parse(size_t len);

  const document &get_document() const {
    if (!is_valid()) {
      throw invalid_json(error);
    }
    return doc;
  }

  static internal::document_parser_implementation *use_implementation(architecture arch);
  static internal::document_parser_implementation *get_implementation() {
    return current_implementation;
  }

private:
  //
  // The maximum document length this parser supports.
  //
  // Buffers are large enough to handle any document up to this length.
  //
  size_t _capacity{0};

  //
  // The maximum depth (number of nested objects and arrays) supported by this parser.
  //
  // Defaults to DEFAULT_MAX_DEPTH.
  //
  size_t _max_depth{0};

  //
  // The implementation that will be used
  //
  static std::atomic<internal::document_parser_implementation*> current_implementation;

  // all nodes are stored on the doc.tape using a 64-bit word.
  //
  // strings, double and ints are stored as
  //  a 64-bit word with a pointer to the actual value
  //
  //
  //
  // for objects or arrays, store [ or {  at the beginning and } and ] at the
  // end. For the openings ([ or {), we annotate them with a reference to the
  // location on the doc.tape of the end, and for then closings (} and ]), we
  // annotate them with a reference to the location of the opening
  //
  //

  // this should be considered a private function
  really_inline void write_tape(uint64_t val, uint8_t c) {
    doc.tape[current_loc++] = val | ((static_cast<uint64_t>(c)) << 56);
  }

  really_inline void annotate_previous_loc(uint32_t saved_loc, uint64_t val) {
    doc.tape[saved_loc] |= val;
  }

  //
  // Set the current capacity: the largest document this parser can support without reallocating.
  //
  // This will allocate *or deallocate* as necessary.
  //
  // Returns false if allocation fails.
  //
  WARN_UNUSED bool set_capacity(size_t capacity);

  //
  // Set the maximum level of nested object and arrays supported by this parser.
  //
  // This will allocate *or deallocate* as necessary.
  //
  // Returns false if allocation fails.
  //
  WARN_UNUSED bool set_max_depth(size_t max_depth);
};

} // namespace simdjson

//
// Inline implementation
//

#include "simdjson/document_parser.h"
#include "simdjson/stage1_find_marks.h"
#include "simdjson/stage2_build_tape.h"

namespace simdjson::internal {

inline document_parser_implementation* document_parser_implementation::get_implementation(architecture arch) {
  switch (arch) {
#ifdef IS_X86_64
    case architecture::HASWELL: return &for_architecture<architecture::HASWELL>::singleton;
    case architecture::WESTMERE: return &for_architecture<architecture::WESTMERE>::singleton;
#endif
#ifdef IS_ARM64
    case architecture::ARM64: return &for_architecture<architecture::ARM64>::singleton;
#endif
    default: return nullptr;
  }
}

inline document_parser_implementation* document_parser_implementation::find_best::get_best_implementation() const {
  return document::parser::use_implementation(find_best_supported_architecture());
}

template<architecture A>
error_code document_parser_implementation::for_architecture<A>::stage1(UNUSED const uint8_t *buf, UNUSED size_t len, document::parser &parser, UNUSED bool streaming) const noexcept {
  return parser.on_error(UNEXPECTED_ERROR);
}
template<architecture A>
error_code document_parser_implementation::for_architecture<A>::stage2(UNUSED const uint8_t *buf, UNUSED size_t len, document::parser &parser) const noexcept {
  return parser.on_error(UNEXPECTED_ERROR);
}
template<architecture A>
error_code document_parser_implementation::for_architecture<A>::stage2(UNUSED const uint8_t *buf, UNUSED size_t len, document::parser &parser, UNUSED size_t &next_json) const noexcept {
  return parser.on_error(UNEXPECTED_ERROR);
}


} // namespace simdjson::internal

namespace simdjson {

inline internal::document_parser_implementation *document::parser::use_implementation(architecture arch) {
  return current_implementation = internal::document_parser_implementation::get_implementation(arch);
}

// TODO inline?
document::doc_ref_result document::parser::parse(const uint8_t *buf, size_t len, bool realloc_if_needed) {
  error_code code = init_parse(len);
  if (code != SUCCESS) { return document::doc_ref_result(doc, code); }

  bool reallocated = false;
  if (realloc_if_needed) {
    const uint8_t *tmp_buf = buf;
    buf = (uint8_t *)allocate_padded_buffer(len);
    if (buf == nullptr)
      return document::doc_ref_result(doc, MEMALLOC);
    memcpy((void *)buf, tmp_buf, len);
    reallocated = true;
  }

  code = current_implementation.load()->stage1(buf, len, *this, false);
  if (code != simdjson::SUCCESS) {
    if (reallocated) { // must free before we exit
      aligned_free((void *)buf);
    }
    return document::doc_ref_result(doc, code);
  }

  code = current_implementation.load()->stage2(buf, len, *this);
  if (reallocated) {
    aligned_free((void *)buf);
  }

  valid = false;
  error = UNINITIALIZED;
  return document::doc_ref_result(doc, code);
}
really_inline document::doc_ref_result document::parser::parse(const char *buf, size_t len, bool realloc_if_needed) {
  return parse((const uint8_t *)buf, len, realloc_if_needed);
}
really_inline document::doc_ref_result document::parser::parse(const std::string &s, bool realloc_if_needed) {
  return parse(s.data(), s.length(), realloc_if_needed);
}
really_inline document::doc_ref_result document::parser::parse(const padded_string &s) {
  return parse(s.data(), s.length(), false);
}

inline document::doc_result document::parse(const uint8_t *buf, size_t len, bool realloc_if_needed) {
  document::parser parser;
  if (!parser.allocate_capacity(len)) {
    return MEMALLOC;
  }
  auto [doc, error] = parser.parse(buf, len, realloc_if_needed);
  return document::doc_result((document &&)doc, error);
}
inline document::doc_result document::parse(const char *buf, size_t len, bool realloc_if_needed) {
    return parse((const uint8_t *)buf, len, realloc_if_needed);
}
inline document::doc_result document::parse(const std::string &s, bool realloc_if_needed) {
    return parse(s.data(), s.length(), realloc_if_needed);
}
inline document::doc_result document::parse(const padded_string &s) {
    return parse(s.data(), s.length(), false);
}

} // namespace simdjson

#endif // SIMDJSON_DOCUMENT_PARSER_H
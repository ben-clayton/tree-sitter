#include "runtime/helpers/spy_input.h"
#include <string.h>
#include <algorithm>
#include "utf8proc.h"

using std::string;

static const size_t UTF8_MAX_CHAR_SIZE = 4;

static long byte_for_character(const char *str, size_t len, size_t goal_character) {
  size_t character = 0, byte = 0;
  int32_t dest_char;

  while (character < goal_character) {
    if (byte >= len)
      return -1;
    byte += utf8proc_iterate(
        (uint8_t *)str + byte,
        len - byte,
        &dest_char);
    character++;
  }

  return byte;
}

SpyInput::SpyInput(string content, size_t chars_per_chunk) :
  chars_per_chunk(chars_per_chunk),
  buffer_size(UTF8_MAX_CHAR_SIZE * chars_per_chunk),
  buffer(new char[buffer_size]),
  byte_offset(0),
  content(content),
  strings_read({""}) {}

SpyInput::~SpyInput() {
  delete buffer;
}

const char * SpyInput::read(void *payload, size_t *bytes_read) {
  auto spy = static_cast<SpyInput *>(payload);

  if (spy->byte_offset > spy->content.size()) {
    *bytes_read = 0;
    return "";
  }

  const char *start = spy->content.data() + spy->byte_offset;
  long byte_count = byte_for_character(start, spy->content.size() - spy->byte_offset, spy->chars_per_chunk);
  if (byte_count < 0)
    byte_count = spy->content.size() - spy->byte_offset;

  *bytes_read = byte_count;
  spy->byte_offset += byte_count;
  spy->strings_read.back() += string(start, byte_count);

  /*
   * This class stores its entire `content` in a contiguous buffer, but we want
   * to ensure that the code under test cannot accidentally read more than
   * `*bytes_read` bytes past the returned pointer. To make sure that this type
   * of error does not fly, we copy the chunk into a zeroed-out buffer and
   * return a reference to that buffer, rather than a pointer into the main
   * content.
   */
  memset(spy->buffer, 0, spy->buffer_size);
  memcpy(spy->buffer, start, byte_count);
  return spy->buffer;
}

int SpyInput::seek(void *payload, TSLength position) {
  auto spy = static_cast<SpyInput *>(payload);
  if (spy->strings_read.size() == 0 || spy->strings_read.back().size() > 0)
    spy->strings_read.push_back("");
  spy->byte_offset = position.bytes;
  return 0;
}

TSInput SpyInput::input() {
  TSInput result;
  result.payload = this;
  result.seek_fn = seek;
  result.read_fn = read;
  return result;
}

bool SpyInput::insert(size_t char_index, string text) {
  long pos = byte_for_character(content.data(), content.size(), char_index);
  if (pos < 0) return false;
  content.insert(pos, text);
  return true;
}

bool SpyInput::erase(size_t char_index, size_t len) {
  long pos = byte_for_character(content.data(), content.size(), char_index);
  if (pos < 0) return false;
  content.erase(pos, len);
  return true;
}

void SpyInput::clear() {
  strings_read.clear();
}

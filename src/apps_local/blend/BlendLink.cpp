#include "BlendLink.h"

#include <cstring>

namespace blend {

namespace {

constexpr char kPrefix[] = "#lb:";
constexpr size_t kPrefixLen = sizeof(kPrefix) - 1;
constexpr size_t kFieldCount = 3;

struct Field {
  char* buf;
  size_t cap;
  size_t len;
  bool cut;
};

// Keeps room for the NUL. Once a byte is dropped, every later byte is dropped
// too, so the field is always a prefix of the input.
void put(Field& f, const char c) {
  if (f.len + 1 < f.cap) {
    f.buf[f.len++] = c;
  } else {
    f.cut = true;
  }
}

// Terminates the field. After a cut the last character may be incomplete: drop
// it whole.
void finish(Field& f) {
  if (f.cut) {
    size_t start = f.len;
    while (start > 0 && (static_cast<unsigned char>(f.buf[start - 1]) & 0xC0) == 0x80) --start;
    if (start > 0) {
      const unsigned char lead = static_cast<unsigned char>(f.buf[start - 1]);
      const size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
      if (f.len - (start - 1) < need) f.len = start - 1;
    }
  }
  f.buf[f.len] = '\0';
}

}  // namespace

bool parseBlendHref(const char* href, BlendGloss& out) {
  out = BlendGloss{};
  if (href == nullptr) return false;

  size_t n = 0;
  while (n < kHrefMax && href[n] != '\0') ++n;
  if (n >= kHrefMax) return false;
  if (std::strncmp(href, kPrefix, kPrefixLen) != 0) return false;

  Field fields[kFieldCount] = {
      {out.lemma, kLemmaCap, 0, false},
      {out.translation, kTranslationCap, 0, false},
      {out.note, kNoteCap, 0, false},
  };
  size_t index = 0;
  for (size_t i = kPrefixLen; i < n; ++i) {
    const char c = href[i];
    if (c == '|') {
      if (++index == kFieldCount) {
        out = BlendGloss{};
        return false;
      }
    } else if (c == '%' && i + 2 < n && href[i + 1] == '7' && (href[i + 2] == 'C' || href[i + 2] == 'c')) {
      put(fields[index], '|');
      i += 2;
    } else if (c == '%' && i + 2 < n && href[i + 1] == '2' && href[i + 2] == '5') {
      put(fields[index], '%');
      i += 2;
    } else {
      put(fields[index], c);  // any other '%' stays literal
    }
  }
  for (Field& f : fields) finish(f);

  if (out.lemma[0] == '\0' || out.translation[0] == '\0') {
    out = BlendGloss{};
    return false;
  }
  return true;
}

}  // namespace blend

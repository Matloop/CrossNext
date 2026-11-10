#pragma once

// Parser for the "blend" EPUB link: <a href="#lb:LEMMA|TRANSLATION|NOTE">form</a>.
// Format and AI rules: docs/blend-format.md.
//
// Freestanding: no Arduino, no heap, no std::string. Fixed buffers, so
// host-tests/blend/ runs it on a laptop. Fields are cut at a UTF-8 character
// boundary, never inside a character.

#include <cstddef>

namespace blend {

// FOOTNOTE_HREF_LEN in lib/Epub/Epub/FootnoteEntry.h, NUL included. Not
// included from there: that header drags in the reader.
constexpr size_t kHrefMax = 256;

// Capacities include the terminating NUL.
constexpr size_t kLemmaCap = 48;
constexpr size_t kTranslationCap = 64;
constexpr size_t kNoteCap = 96;

struct BlendGloss {
  char lemma[kLemmaCap] = {};
  char translation[kTranslationCap] = {};
  char note[kNoteCap] = {};
};

// True when `href` is a well-formed blend link. `out` is cleared first and is
// left empty on failure. False when: the href is null, has no NUL inside
// kHrefMax bytes, does not start with "#lb:", has more than three fields, or has
// an empty lemma or translation. A missing or empty note is valid.
bool parseBlendHref(const char* href, BlendGloss& out);

}  // namespace blend

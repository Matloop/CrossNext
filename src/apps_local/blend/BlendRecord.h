#pragma once

// Maps a tapped blend word onto the Words store (src/apps_local/words). Pure
// functions, no device calls, so host-tests/blend/ runs them with the real
// vocabulary::capture. Format and saving rules: docs/blend-format.md.

#include <cstdint>
#include <string>

#include "BlendLink.h"
#include "apps_local/words/Vocabulary.h"

namespace blend {

// Words key = (dictionary, canonical headword). All blend words share this one.
constexpr const char* kDictionary = "Blend";

// Where the reader was when the word was tapped. Pointers only need to live
// until fillContext() returns.
struct BlendSource {
  const char* path;
  const char* book;
  const char* author;
  const char* chapter;
  const char* language;
  uint16_t spine;
  uint16_t page;
  uint32_t offset;  // visible-codepoint offset of the page start, or UINT32_MAX
};

// Lemma with ASCII letters in lower case, the way Words folds them for its id.
// Words refuses a save whose headword differs from the stored one by bytes.
std::string headword(const BlendGloss& gloss);

// "translation" or "translation (note)".
std::string meaning(const BlendGloss& gloss);

// Fills a blank `ctx`. The page link text is not reachable from the reader's
// link list, so the lemma stands in for the form and the passage, labelled as a
// visible-passage fragment. False when a field does not fit.
bool fillContext(vocabulary::Context& ctx, const BlendGloss& gloss, const BlendSource& source);

}  // namespace blend

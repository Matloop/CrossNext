#include "BlendRecord.h"

namespace blend {

std::string headword(const BlendGloss& gloss) {
  std::string out = gloss.lemma;
  for (auto& c : out)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
  return out;
}

std::string meaning(const BlendGloss& gloss) {
  std::string out = gloss.translation;
  if (gloss.note[0]) out += std::string(" (") + gloss.note + ")";
  return out;
}

bool fillContext(vocabulary::Context& ctx, const BlendGloss& gloss, const BlendSource& s) {
  ctx = {};
  ctx.fragment = true;
  ctx.spine = s.spine;
  ctx.page = s.page;
  ctx.offset = s.offset;
  return vocabulary::copy(ctx.path, sizeof(ctx.path), s.path) && vocabulary::copy(ctx.book, sizeof(ctx.book), s.book) &&
         vocabulary::copy(ctx.author, sizeof(ctx.author), s.author) &&
         vocabulary::copy(ctx.chapter, sizeof(ctx.chapter), s.chapter) &&
         vocabulary::copy(ctx.surface, sizeof(ctx.surface), gloss.lemma) &&
         vocabulary::copy(ctx.sentence, sizeof(ctx.sentence), gloss.lemma);
}

}  // namespace blend

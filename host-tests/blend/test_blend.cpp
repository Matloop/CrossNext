// Blend link parser tests. Freestanding: no device, no PlatformIO.

#include <cstdio>
#include <cstring>
#include <string>

#include "../../src/apps_local/blend/BlendLink.h"

namespace {

int checksRun = 0;
int checksFailed = 0;

void check(const bool condition, const char* what, const int line) {
  checksRun++;
  if (condition) return;
  checksFailed++;
  std::printf("FAIL test_blend.cpp:%d  %s\n", line, what);
}

#define CHECK(expr) check((expr), #expr, __LINE__)

using namespace blend;

// Independent UTF-8 validity check (shortest-form and range rules left out:
// the parser only has to never leave a lead byte without its continuations).
bool wellFormedUtf8(const char* s) {
  const size_t n = std::strlen(s);
  size_t i = 0;
  while (i < n) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    size_t len = 1;
    if (c >= 0xF0) {
      len = 4;
    } else if (c >= 0xE0) {
      len = 3;
    } else if (c >= 0xC0) {
      len = 2;
    } else if (c >= 0x80) {
      return false;  // stray continuation byte
    }
    if (i + len > n) return false;
    for (size_t k = 1; k < len; ++k) {
      if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80) return false;
    }
    i += len;
  }
  return true;
}

void testHappyPath() {
  BlendGloss g;
  CHECK(parseBlendHref("#lb:sorridere|sorrir|verbo", g));
  CHECK(std::strcmp(g.lemma, "sorridere") == 0);
  CHECK(std::strcmp(g.translation, "sorrir") == 0);
  CHECK(std::strcmp(g.note, "verbo") == 0);
}

void testNoteIsOptional() {
  BlendGloss g;
  CHECK(parseBlendHref("#lb:casa|casa", g));
  CHECK(std::strcmp(g.lemma, "casa") == 0);
  CHECK(std::strcmp(g.translation, "casa") == 0);
  CHECK(g.note[0] == '\0');

  // A trailing separator is an empty note, still valid.
  BlendGloss h;
  CHECK(parseBlendHref("#lb:casa|casa|", h));
  CHECK(h.note[0] == '\0');
}

void testEscapes() {
  BlendGloss g;
  CHECK(parseBlendHref("#lb:a%7Cb|x%25y|n%7C%25", g));
  CHECK(std::strcmp(g.lemma, "a|b") == 0);
  CHECK(std::strcmp(g.translation, "x%y") == 0);
  CHECK(std::strcmp(g.note, "n|%") == 0);

  BlendGloss lower;
  CHECK(parseBlendHref("#lb:a%7cb|x", lower));
  CHECK(std::strcmp(lower.lemma, "a|b") == 0);

  // %7C is a character, not a separator: it must not split fields.
  BlendGloss one;
  CHECK(parseBlendHref("#lb:%7C|%7C", one));
  CHECK(std::strcmp(one.lemma, "|") == 0);
  CHECK(std::strcmp(one.translation, "|") == 0);

  // Only %7C and %25 are escapes. Anything else stays as written.
  BlendGloss other;
  CHECK(parseBlendHref("#lb:50%41|100%", other));
  CHECK(std::strcmp(other.lemma, "50%41") == 0);
  CHECK(std::strcmp(other.translation, "100%") == 0);

  // %25 decodes once: "%257C" is "%" then "7C".
  BlendGloss twice;
  CHECK(parseBlendHref("#lb:%257C|x", twice));
  CHECK(std::strcmp(twice.lemma, "%7C") == 0);
}

void testRejectsEmptyRequiredField() {
  BlendGloss g;
  CHECK(!parseBlendHref("#lb:", g));
  CHECK(!parseBlendHref("#lb:casa", g));
  CHECK(!parseBlendHref("#lb:|casa", g));
  CHECK(!parseBlendHref("#lb:casa|", g));
  CHECK(!parseBlendHref("#lb:casa||nota", g));
  CHECK(!parseBlendHref("#lb:||", g));
  // A fourth field is malformed, not silently dropped.
  CHECK(!parseBlendHref("#lb:a|b|c|d", g));
  // Failure leaves nothing behind.
  CHECK(g.lemma[0] == '\0' && g.translation[0] == '\0' && g.note[0] == '\0');
}

void testRejectsWrongPrefix() {
  BlendGloss g;
  CHECK(!parseBlendHref(nullptr, g));
  CHECK(!parseBlendHref("", g));
  CHECK(!parseBlendHref("#lb", g));
  CHECK(!parseBlendHref("#fn:a|b", g));
  CHECK(!parseBlendHref("lb:a|b", g));
  CHECK(!parseBlendHref("#LB:a|b", g));
  CHECK(!parseBlendHref(" #lb:a|b", g));
  CHECK(!parseBlendHref("chapter2.xhtml#lb:a|b", g));
}

void testFailureClearsPreviousResult() {
  BlendGloss g;
  CHECK(parseBlendHref("#lb:a|b|c", g));
  CHECK(!parseBlendHref("#fn:a|b|c", g));
  CHECK(g.lemma[0] == '\0' && g.translation[0] == '\0' && g.note[0] == '\0');
}

// FOOTNOTE_HREF_LEN is 256 including the NUL, so the longest href is 255 chars.
void testHrefLengthLimit() {
  std::string href = "#lb:" + std::string(20, 'l') + "|" + std::string(30, 't') + "|";
  href += std::string(255 - href.size(), 'n');
  CHECK(href.size() == 255);
  BlendGloss g;
  CHECK(parseBlendHref(href.c_str(), g));
  CHECK(std::strlen(g.lemma) == 20);
  CHECK(std::strlen(g.translation) == 30);
  // The note is longer than its buffer: cut, still terminated.
  CHECK(std::strlen(g.note) == kNoteCap - 1);

  // 256 characters do not fit with their NUL: refused.
  const std::string tooLong = href + "n";
  CHECK(!parseBlendHref(tooLong.c_str(), g));

  // A buffer with no NUL in its 256 bytes is never read past its end.
  char unterminated[kHrefMax];
  std::memset(unterminated, 'x', sizeof(unterminated));
  std::memcpy(unterminated, "#lb:a|b|", 8);
  CHECK(!parseBlendHref(unterminated, g));
}

void testCutsAtUtf8Boundary() {
  // "à" is two bytes. 46 ASCII bytes + "à" is 48 bytes against a 47-byte lemma:
  // the lead byte fits, its continuation does not, so the whole character goes.
  BlendGloss g;
  const std::string href = "#lb:" + std::string(kLemmaCap - 2, 'a') + "\xC3\xA0|x";
  CHECK(parseBlendHref(href.c_str(), g));
  CHECK(std::strlen(g.lemma) == kLemmaCap - 2);
  CHECK(wellFormedUtf8(g.lemma));

  // Exactly fits: nothing is lost.
  BlendGloss fits;
  const std::string exact = "#lb:" + std::string(kLemmaCap - 3, 'a') + "\xC3\xA0|x";
  CHECK(parseBlendHref(exact.c_str(), fits));
  CHECK(std::strlen(fits.lemma) == kLemmaCap - 1);
  CHECK(std::strcmp(fits.lemma + kLemmaCap - 3, "\xC3\xA0") == 0);

  // Every offset around the limit, for 2-, 3- and 4-byte characters, in each
  // field in turn (the other two stay short so the href fits in 255 bytes):
  // always well formed, always a prefix of the input, never empty.
  const char* chars[] = {"\xC3\xA0", "\xE2\x82\xAC", "\xF0\x9F\x98\x80"};
  for (const char* ch : chars) {
    for (size_t which = 0; which < 3; ++which) {
      for (size_t pad = 30; pad < 110; ++pad) {
        const std::string body = std::string(pad, 'a') + ch + ch;
        std::string parts[3] = {"l", "t", "n"};
        parts[which] = body;
        const std::string full = "#lb:" + parts[0] + "|" + parts[1] + "|" + parts[2];
        BlendGloss r;
        CHECK(parseBlendHref(full.c_str(), r));
        const char* got[3] = {r.lemma, r.translation, r.note};
        for (size_t f = 0; f < 3; ++f) {
          CHECK(wellFormedUtf8(got[f]));
          CHECK(std::strlen(got[f]) > 0);
          CHECK(parts[f].compare(0, std::strlen(got[f]), got[f]) == 0);
        }
      }
    }
  }
}

void testItalianAccents() {
  BlendGloss g;
  CHECK(parseBlendHref("#lb:perch\xC3\xA9|porque|congiun\xC3\xA7\xC3\xA3o", g));
  CHECK(std::strcmp(g.lemma, "perch\xC3\xA9") == 0);
  CHECK(std::strcmp(g.translation, "porque") == 0);
  CHECK(std::strcmp(g.note, "congiun\xC3\xA7\xC3\xA3o") == 0);

  // Long Italian lemma with an accent right at the limit.
  BlendGloss cut;
  const std::string lemma = std::string(kLemmaCap - 2, 's') + "\xC3\xA0" + "orridere";
  const std::string href = "#lb:" + lemma + "|sorrir|verbo";
  CHECK(parseBlendHref(href.c_str(), cut));
  CHECK(wellFormedUtf8(cut.lemma));
  CHECK(std::strcmp(cut.translation, "sorrir") == 0);
  CHECK(std::strcmp(cut.note, "verbo") == 0);
}

}  // namespace

int main() {
  testHappyPath();
  testNoteIsOptional();
  testEscapes();
  testRejectsEmptyRequiredField();
  testRejectsWrongPrefix();
  testFailureClearsPreviousResult();
  testHrefLengthLimit();
  testCutsAtUtf8Boundary();
  testItalianAccents();

  std::printf("%d checks, %d failed\n", checksRun, checksFailed);
  return checksFailed == 0 ? 0 : 1;
}

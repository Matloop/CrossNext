// Blend -> Words record tests. Runs the real vocabulary::capture/undo against an
// in-memory store. No device and no PlatformIO.

#include <cstdio>
#include <cstring>
#include <map>
#include <string>

#include "../../src/apps_local/blend/BlendRecord.h"

namespace {

int checksRun = 0;
int checksFailed = 0;

void check(const bool condition, const char* what, const int line) {
  checksRun++;
  if (condition) return;
  checksFailed++;
  std::printf("FAIL test_record.cpp:%d  %s\n", line, what);
}

#define CHECK(expr) check((expr), #expr, __LINE__)

using namespace vocabulary;

struct Fake final : Records {
  std::map<std::string, Word> words;
  std::map<std::string, Context> contexts;
  int failAtWrite = 0, writes = 0;
  Read word(const char* id, Word& w) override {
    auto i = words.find(id);
    if (i == words.end()) return Read::Missing;
    w = i->second;
    return Read::Ok;
  }
  Read context(const char* id, Context& c) override {
    auto i = contexts.find(id);
    if (i == contexts.end()) return Read::Missing;
    c = i->second;
    return Read::Ok;
  }
  bool put(const Word& w) override {
    if (++writes == failAtWrite) return false;
    words[w.id] = w;
    return true;
  }
  bool put(const Context& c) override {
    if (++writes == failAtWrite) return false;
    contexts[c.id] = c;
    return true;
  }
  bool removeWord(const char* id) override {
    words.erase(id);
    return true;
  }
  bool removeContext(const char* id) override {
    contexts.erase(id);
    return true;
  }
};

blend::BlendGloss gloss(const char* lemma, const char* translation, const char* note) {
  blend::BlendGloss g;
  CHECK(blend::parseBlendHref((std::string("#lb:") + lemma + "|" + translation + "|" + note).c_str(), g));
  return g;
}

const blend::BlendSource kSource{"/books/a.epub", "Livro", "Autora", "Capitulo 1", "pt", 3, 7, 1200};

// What vocabulary::saveLookup does with these arguments, minus storage and HTML.
Capture save(Fake& db, const blend::BlendGloss& g, const blend::BlendSource& s, Undo& token) {
  Context ctx;
  CHECK(blend::fillContext(ctx, g, s));
  Word incoming;
  const std::string head = blend::headword(g);
  const std::string mean = blend::meaning(g);
  CHECK(copy(incoming.surface, sizeof(incoming.surface), ctx.surface));
  CHECK(copy(incoming.headword, sizeof(incoming.headword), head.c_str()));
  CHECK(copy(incoming.meaning, sizeof(incoming.meaning), mean.c_str()));
  CHECK(copy(incoming.dictionary, sizeof(incoming.dictionary), blend::kDictionary));
  CHECK(copy(incoming.language, sizeof(incoming.language), s.language));
  Word oldWord;
  Context oldContext;
  return capture(db, incoming, ctx, oldWord, oldContext, token);
}

void testHeadwordAndMeaning() {
  const auto g = gloss("Il Mare", "o mar", "masculino");
  CHECK(blend::headword(g) == "il mare");
  CHECK(blend::meaning(g) == "o mar (masculino)");
  CHECK(blend::meaning(gloss("sorridere", "sorrir", "")) == "sorrir");
}

void testFillContext() {
  Context ctx;
  CHECK(blend::fillContext(ctx, gloss("il mare", "o mar", ""), kSource));
  CHECK(std::strcmp(ctx.path, "/books/a.epub") == 0);
  CHECK(std::strcmp(ctx.book, "Livro") == 0);
  CHECK(std::strcmp(ctx.author, "Autora") == 0);
  CHECK(std::strcmp(ctx.chapter, "Capitulo 1") == 0);
  CHECK(std::strcmp(ctx.surface, "il mare") == 0);
  CHECK(std::strcmp(ctx.sentence, "il mare") == 0);
  CHECK(ctx.spine == 3 && ctx.page == 7 && ctx.offset == 1200);
  CHECK(ctx.fragment);
  CHECK(ctx.purpose == Purpose::Vocabulary);
}

void testFillContextRejectsOversizedPath() {
  const std::string longPath = "/" + std::string(600, 'a');
  auto s = kSource;
  s.path = longPath.c_str();
  Context ctx;
  CHECK(!blend::fillContext(ctx, gloss("a", "b", ""), s));
}

void testSavesNewWord() {
  Fake db;
  Undo token;
  CHECK(save(db, gloss("il mare", "o mar", "masculino"), kSource, token) == Capture::Saved);
  CHECK(db.words.size() == 1 && db.contexts.size() == 1);
  const Word& w = db.words.begin()->second;
  CHECK(std::strcmp(w.dictionary, "Blend") == 0);
  CHECK(std::strcmp(w.headword, "il mare") == 0);
  CHECK(std::strcmp(w.meaning, "o mar (masculino)") == 0);
  CHECK(w.encounters == 1);
  CHECK(token.valid);
}

void testSavingAgainKeepsFsrsAndCountsEncounter() {
  Fake db;
  Undo token;
  CHECK(save(db, gloss("il mare", "o mar", ""), kSource, token) == Capture::Saved);
  Word& stored = db.words.begin()->second;
  stored.card.state = 2;
  stored.card.stability = 12.5f;
  stored.card.dueDay = 9000;
  stored.card.reps = 4;
  // Same lemma in another case and a different note: same record.
  CHECK(save(db, gloss("Il Mare", "o mar", "nota"), kSource, token) == Capture::Updated);
  CHECK(db.words.size() == 1 && db.contexts.size() == 1);
  const Word& after = db.words.begin()->second;
  CHECK(after.encounters == 2);
  CHECK(after.card.state == 2 && after.card.stability == 12.5f && after.card.dueDay == 9000 && after.card.reps == 4);
  CHECK(std::strcmp(after.meaning, "o mar") == 0);
}

void testOtherChapterAddsContext() {
  Fake db;
  Undo token;
  CHECK(save(db, gloss("il mare", "o mar", ""), kSource, token) == Capture::Saved);
  auto other = kSource;
  other.spine = 5;
  CHECK(save(db, gloss("il mare", "o mar", ""), other, token) == Capture::Updated);
  CHECK(db.words.size() == 1 && db.contexts.size() == 2);
}

void testFailureCreatesNothing() {
  for (int failAt = 1; failAt <= 2; failAt++) {
    Fake db;
    db.failAtWrite = failAt;
    Undo token;
    CHECK(save(db, gloss("il mare", "o mar", ""), kSource, token) == Capture::Failed);
    CHECK(db.words.empty() && db.contexts.empty());
    CHECK(!token.valid);
  }
}

void testUndoRemovesNewWordAndKeepsOldOnRepeat() {
  Fake db;
  Undo token;
  Word scratch;
  CHECK(save(db, gloss("il mare", "o mar", ""), kSource, token) == Capture::Saved);
  CHECK(undo(db, token, scratch));
  CHECK(db.words.empty() && db.contexts.empty());

  CHECK(save(db, gloss("il mare", "o mar", ""), kSource, token) == Capture::Saved);
  CHECK(save(db, gloss("il mare", "o mar", ""), kSource, token) == Capture::Updated);
  CHECK(undo(db, token, scratch));
  CHECK(db.words.size() == 1 && db.words.begin()->second.encounters == 1);
}

}  // namespace

int main() {
  testHeadwordAndMeaning();
  testFillContext();
  testFillContextRejectsOversizedPath();
  testSavesNewWord();
  testSavingAgainKeepsFsrsAndCountsEncounter();
  testOtherChapterAddsContext();
  testFailureCreatesNothing();
  testUndoRemovesNewWordAndKeepsOldOnRepeat();

  std::printf("%d checks, %d failed\n", checksRun, checksFailed);
  return checksFailed == 0 ? 0 : 1;
}

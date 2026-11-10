#pragma once

// Footer strip for a tapped blend word: lemma, translation, note and a Save
// button, painted straight onto the reader's framebuffer. The reader owns the
// refresh and the page snapshot (Overlay::Blend in EpubReaderActivity); this
// class only draws and reads input. Save stores the word in Words and turns
// into Undo. Format: docs/blend-format.md.

#include <memory>

// VocabularyStorage.h first: HalStorage.h pulls Arduino.h, whose word() macro
// must already be active when Vocabulary.h declares Records::word.
#include "apps_local/words/VocabularyStorage.h"
// clang-format off
#include "BlendLink.h"
#include "BlendRecord.h"
// clang-format on

class GfxRenderer;
class MappedInputManager;

namespace blend {

class BlendFooter {
 public:
  // Redraw: the saved state changed, repaint the strip.
  enum class Result { None, Close, Redraw };

  // `tapY`: where the word was touched. The strip moves to the top when it
  // would cover that point. `source` is copied; a save without room for the
  // copy reports a failed save.
  void open(const BlendGloss& value, int tapY, const BlendSource& source);

  // Caller holds the RenderLock and pushes the refresh.
  void paint(GfxRenderer& renderer);

  // Close on Back, Confirm or a tap outside the strip. A tap on Save/Undo
  // saves or undoes and returns Redraw.
  Result handleInput(const MappedInputManager& input);

 private:
  struct Box {
    int x = 0, y = 0, w = 0, h = 0;
    bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
  };

  // Words save, or undo of the save just made.
  void saveOrUndo();

  BlendGloss gloss;
  int tapY = 0;
  Box strip, saveButton;  // set by paint(), read by handleInput()
  std::unique_ptr<vocabulary::Context> context;  // null: no room at open(), saving fails
  char language[32] = {};
  vocabulary::CaptureNotice notice;
  bool undone = false;
};

}  // namespace blend

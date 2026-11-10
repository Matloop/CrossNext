#include "BlendFooter.h"

#include <algorithm>
#include <string>
#include <vector>

#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include "GfxRenderer.h"
#include "MappedInputManager.h"
#include "apps_local/ui/PixelMenu.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace blend {

namespace {
constexpr int kPad = 12;
constexpr int kGap = 6;
constexpr int kMaxLines = 2;
}  // namespace

void BlendFooter::open(const BlendGloss& value, const int touchY, const BlendSource& source) {
  gloss = value;
  tapY = touchY;
  strip = {};
  saveButton = {};
  notice = {};
  undone = false;
  vocabulary::copy(language, sizeof(language), source.language);
  context = makeUniqueNoThrow<vocabulary::Context>();
  if (!context) {
    LOG_ERR("WORDS", "OOM: blend source context %u bytes", unsigned(sizeof(vocabulary::Context)));
  } else if (!fillContext(*context, gloss, source)) {
    context.reset();
  }
}

void BlendFooter::saveOrUndo() {
  if (notice.token.valid) {
    undone = vocabulary::undoLookup(notice.token);
    if (!undone) notice.status = vocabulary::Capture::Failed;
    return;
  }
  undone = false;
  if (!context) {
    notice = {};
    notice.attempted = true;
    notice.status = vocabulary::Capture::Failed;
    return;
  }
  notice = vocabulary::saveLookup(*context, context->surface, headword(gloss).c_str(), meaning(gloss), false,
                                  kDictionary, language);
}

void BlendFooter::paint(GfxRenderer& renderer) {
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer);
  const int textW = safe.width - 2 * kPad;

  const std::string lemma = renderer.truncatedText(UI_12_FONT_ID, gloss.lemma, textW, EpdFontFamily::BOLD);
  const auto translation = renderer.wrappedText(NOTOSERIF_16_FONT_ID, gloss.translation, textW, kMaxLines);
  std::vector<std::string> note;
  if (gloss.note[0]) note = renderer.wrappedText(SMALL_FONT_ID, gloss.note, textW, kMaxLines);

  const int lemmaH = renderer.getLineHeight(UI_12_FONT_ID);
  const int translationH = renderer.getLineHeight(NOTOSERIF_16_FONT_ID);
  const int noteH = renderer.getLineHeight(SMALL_FONT_ID);
  const int buttonH = renderer.getLineHeight(UI_10_FONT_ID) + 2 * kPad;

  const int height = std::min(safe.height, kPad + lemmaH + kGap + static_cast<int>(translation.size()) * translationH +
                                               (note.empty() ? 0 : kGap + static_cast<int>(note.size()) * noteH) +
                                               kGap + buttonH + kPad);
  const bool atTop = tapY >= safe.y + safe.height - height;
  strip = {safe.x, atTop ? safe.y : safe.y + safe.height - height, safe.width, height};

  renderer.fillRect(strip.x, strip.y, strip.w, strip.h, false);
  renderer.drawRect(strip.x, strip.y, strip.w, strip.h, 1, true);

  const int x = strip.x + kPad;
  int y = strip.y + kPad;
  renderer.drawText(UI_12_FONT_ID, x, y, lemma.c_str(), true, EpdFontFamily::BOLD);
  y += lemmaH + kGap;
  for (const auto& line : translation) {
    renderer.drawText(NOTOSERIF_16_FONT_ID, x, y, line.c_str());
    y += translationH;
  }
  if (!note.empty()) {
    y += kGap;
    for (const auto& line : note) {
      renderer.drawText(SMALL_FONT_ID, x, y, line.c_str());
      y += noteH;
    }
  }
  y += kGap;

  // Same frame as the Words buttons: black outline, pixelui::kBorder thick.
  const char* label = notice.token.valid ? tr(STR_WORDS_UNDO) : tr(STR_WORDS_SAVE_WORD);
  const int labelW = renderer.getTextWidth(UI_10_FONT_ID, label);
  const int buttonW = std::max(safe.width / 3, labelW + 4 * kPad);
  saveButton = {strip.x + strip.w - kPad - buttonW, y, buttonW, buttonH};
  renderer.drawRect(saveButton.x, saveButton.y, saveButton.w, saveButton.h, pixelui::kBorder, true);
  renderer.drawText(UI_10_FONT_ID, saveButton.x + (buttonW - labelW) / 2,
                    saveButton.y + (buttonH - renderer.getLineHeight(UI_10_FONT_ID)) / 2, label);

  if (notice.attempted) {
    const bool failed =
        notice.status == vocabulary::Capture::Failed || notice.status == vocabulary::Capture::Rejected;
    const char* message =
        undone ? tr(STR_WORDS_UNDONE) : failed ? tr(STR_WORDS_NOT_SAVED) : tr(STR_WORDS_SAVED_LABEL);
    const std::string fitted =
        renderer.truncatedText(SMALL_FONT_ID, message, saveButton.x - kGap - x, EpdFontFamily::REGULAR);
    renderer.drawText(SMALL_FONT_ID, x, saveButton.y + (buttonH - renderer.getLineHeight(SMALL_FONT_ID)) / 2,
                      fitted.c_str());
  }
}

BlendFooter::Result BlendFooter::handleInput(const MappedInputManager& input) {
  if (input.wasReleased(MappedInputManager::Button::Back) || input.wasReleased(MappedInputManager::Button::Confirm)) {
    return Result::Close;
  }
  int x = 0;
  int y = 0;
  if (!input.wasScreenTapped(x, y)) return Result::None;
  if (saveButton.contains(x, y)) {
    saveOrUndo();
    return Result::Redraw;
  }
  return strip.contains(x, y) ? Result::None : Result::Close;
}

}  // namespace blend

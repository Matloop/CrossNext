# Blend link format

A "blend" EPUB mixes words of a target language into a book written in the
reader's base language. Each blended word is a link that carries its own gloss,
so the device needs no dictionary to show it.

Parser: `src/apps_local/blend/BlendLink.{h,cpp}`. Tests: `host-tests/blend/run.sh`.

## Link

```html
<a href="#lb:LEMMA|TRANSLATION|NOTE">form-in-the-book</a>
```

- Link text: the word as it appears in the sentence (inflected form).
- `LEMMA`: dictionary form in the target language. Required.
- `TRANSLATION`: short meaning in the base language. Required.
- `NOTE`: optional (grammar, register, gender). The third field may be missing or empty.
- Fields are raw UTF-8, separated by `|`.
- Inside a field, `|` is written `%7C` and `%` is written `%25`. Nothing else is escaped.
- Keep quotes and `&` valid for XML: `&amp;`, `&quot;`.
- The whole `href` must be at most 255 bytes (256 with the NUL, `FOOTNOTE_HREF_LEN`).
  A longer href is not parsed.

Parser limits (bytes, UTF-8, cut at a character boundary): lemma 47,
translation 63, note 95. Keep fields much shorter than this.

A link is rejected when the prefix is not `#lb:`, when LEMMA or TRANSLATION is
empty, or when there are more than three fields.

## Reader behaviour

A tap on a link whose `href` parses as a blend link does not navigate. The
reader paints a footer strip over the page (`BlendFooter`): lemma in bold,
translation larger, note small, and a Save button with a thick border. The
strip goes to the top when the tapped word is in the bottom area, so the word
stays visible. Back, Confirm or a tap outside the strip closes it. Any other
link keeps its normal behaviour (footnote or jump).

Save puts the word in Words (`BlendRecord`, `vocabulary::saveLookup`):

- Dictionary: the fixed name `Blend`. Words keys a word by dictionary and
  canonical headword, so every blend word shares one pseudo-dictionary.
- Headword: the lemma, ASCII letters in lower case (Words folds case in its id
  and refuses a save whose bytes differ from the stored headword).
- Meaning: `translation`, or `translation (note)`.
- Book, author, chapter, spine, page, visible offset and book language come
  from the reader. The offset is the start of the page, not of the word.
- Passage and selected form: the page link carries no text, so both are the
  lemma, and the passage is labelled as a visible fragment. A later step can
  read the real form and sentence from the page.
- The button turns into Undo and a status line appears left of it: Saved, Not
  saved (allocation, SD or length failure; nothing is created) or Removed.
- Saving a word again in the same chapter raises its encounter count only. The
  FSRS state and the saved meaning stay as they were. Another chapter or book
  adds a context.

## Rules for the AI

- Ask the user for: base language, target language, level (A1-C2), and percentage.
- Level: blend only words a learner at that level can guess from context or
  should learn next. Lower level, more common words and shorter phrases.
- Percentage: share of running words to blend, over the whole book. Spread the
  words evenly. Do not blend two sentences in a row at the same density.
- Do not blend proper nouns (people, places, brands, titles).
- Do not blend inside headings, the table of contents, or existing links.
- Blend a fixed expression as one unit: one link for the whole phrase, not one
  per word. The lemma is the phrase in its dictionary form.
- Lemma in dictionary form: infinitive for verbs, singular for nouns, masculine
  singular for adjectives.
- Translation: short, in the base language, in the sense used in this sentence.
  One to three words. Put detail in NOTE.
- Keep the sentence readable in the base language: the blended word must fit the
  grammar around it. If it does not, do not blend it.
- Do not change anything else: keep the XHTML structure, ids, and other links.
- Output well-formed XHTML.

## Example

Base language PT-BR, target language Italian, level A2.

Before:

```html
<p>Ela ficou olhando para o mar e começou a sorrir.</p>
```

After:

```html
<p>Ela ficou olhando para <a href="#lb:il mare|o mar|masculino">il mare</a> e começou a
<a href="#lb:sorridere|sorrir|verbo">sorridere</a>.</p>
```

The article and noun are one link, so the base article is replaced too.

## Prompt

Paste this into an AI, then attach the EPUB or text.

```text
You will receive an EPUB chapter (XHTML) or plain text. Return the same content
as well-formed XHTML, with some words replaced by words in the TARGET language,
each one wrapped in a link.

BASE language: Brazilian Portuguese (PT-BR)
TARGET language: Italian
LEVEL: A2
PERCENTAGE: 8% of the words

Link format, exactly:
<a href="#lb:LEMMA|TRANSLATION|NOTE">form</a>
- form (the link text): the target-language word as it appears in the sentence.
- LEMMA: dictionary form in the target language (verb infinitive, noun singular,
  adjective masculine singular). Required.
- TRANSLATION: short meaning in the BASE language, 1 to 3 words, in the sense
  used in this sentence. Required.
- NOTE: optional, very short (gender, verb, register). May be omitted with its "|".
- Write fields as plain UTF-8. Inside a field, write "|" as %7C and "%" as %25.
- The whole href must be at most 255 bytes.

Rules:
- Replace only words a learner at LEVEL can guess from context or should learn next.
- Reach about PERCENTAGE of the words, spread evenly through the text.
- Never replace proper nouns (people, places, brands, titles).
- Never replace words inside headings or inside existing links.
- A fixed expression is one unit: one link for the whole phrase, with the phrase
  as LEMMA.
- The sentence must stay grammatical: if the target word does not fit the
  surrounding base-language grammar, leave it alone.
- Keep all other markup, ids and links unchanged. Do not add comments or
  explanations. Output only the XHTML.

Example. Input:
<p>Ela ficou olhando para o mar e começou a sorrir.</p>
Output:
<p>Ela ficou olhando para <a href="#lb:il mare|o mar|masculino">il mare</a> e começou a <a href="#lb:sorridere|sorrir|verbo">sorridere</a>.</p>
```

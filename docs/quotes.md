# Quotes

Short passages from Marcus Aurelius, Seneca, Epictetus and Epicurus appear on the Today card while
a block runs, under the timer in focus mode and on the block start, break, push-up and block done
overlays. They are decorative: a quote never takes focus, never sits under a button and never
changes on a tick. The Settings screen turns them off, or adds the Latin or Greek original under
the translation on the overlays.

## Data

`resources/quotes/quotes.json` is embedded as `:/quotes/quotes.json` and validated at startup by
`parseQuoteCatalog` in `src/core`. It holds:

- `authors` and `works`: keyed by a short id, with `en`, `es` and `fr` names.
- `quotes`: one entry per quote with `id` (unique), `context` (`blockStart`, `focus`, `break`,
  `pushups` or `blockEnd`), `author` and `work` (keys into the maps above), `locus`, `lang` (`la`
  or `grc`), `original`, the three renderings `en`, `es`, `fr` and, optionally, `citing` with the
  three languages when the author quotes someone else.

Every context must have at least one quote. The renderings are our own, made from the Greek or
Latin originals; do not paste a modern copyrighted translation. In French, put a no-break space
(U+00A0) before `: ; ? !`.

## Adding a quote

1. Append an entry to `quotes` with a new id, such as `seneca-ep-49`, and the fields above. Add
   the author or work to the maps first if it is new.
2. Check the original against a public critical text (Perseus, The Latin Library, Wikisource).
   Write Greek elisions with the koronis (U+1FBD, `᾽`), the one apostrophe form used in the
   file, and end a cut original with ` …` when the rendering continues past it.
3. Run the core tests: `tests/core/quotes_test.cpp` loads the shipped file and checks every field,
   the unique ids and the contexts.
4. Start a debug build against a test session and look at the surface the context belongs to. A
   quote that does not fit its slot is skipped for the next one of its group, so a long quote may
   simply never appear on the Today card; keep them short.

The original line is drawn in Noto Serif Italic, subset to Latin, Greek and Greek Extended under
`src/app/fonts/NotoSerif`. Check that every character of a new original exists in that subset.

## Selection

`ShuffleBag` in core draws every quote of a context once before any repeats and never opens a new
round with the quote that closed the previous one. The bag state of every context is written to
`quotes-state.json` under the data directory after each draw, so a restart continues the round.

The `Quotes` singleton remembers one quote per surface and phase key. A surface (`QuoteBlock`
with `surface` and `phaseKey`) draws when its key first appears and gets the same id back for as
long as the key lasts, so a re-created delegate, a re-render or a language change never changes the
quote; the language only swaps the wording. The attribution reads `AUTHOR · WORK, LOCUS`, or
`AUTHOR, CITING X · WORK LOCUS`, uppercased by the component.

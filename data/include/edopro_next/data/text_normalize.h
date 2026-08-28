// Case- and accent-folding for search comparisons, source-faithful to
// upstream's Utils::ToUpperChar/ToUpperNoAccents (gframe/utils.h) - see
// docs/architecture/card-search.md#normalization for the exact codepoint
// table this reproduces and why. UTF-8 in, UTF-8 out; no wchar_t, no
// Irrlicht, no locale dependency of any kind (a deliberate, harmless
// strengthening over upstream's own std::toupper fallback - see the .cpp).
#ifndef EDOPRO_NEXT_DATA_TEXT_NORMALIZE_H
#define EDOPRO_NEXT_DATA_TEXT_NORMALIZE_H

#include <string>
#include <string_view>

namespace edopro_next::data {

// Uppercases ASCII letters and folds a small, explicit set of Latin-1
// accented letters and two punctuation marks to their unaccented ASCII
// equivalents - exactly the set upstream's ToUpperChar handles, no more
// and no less. A codepoint outside that set (any non-Latin script, and
// any Latin-1/Latin Extended character not in upstream's own table, such
// as Æ or ß) passes through unchanged, matching upstream's real observable
// behaviour - not a Unicode-complete case-folding implementation, and not
// meant to be one. Any byte sequence that is not strictly canonical,
// well-formed UTF-8 (a stray continuation byte, a truncated multi-byte
// sequence, an overlong encoding, a UTF-16 surrogate codepoint, anything
// beyond U+10FFFF) is copied through byte-for-byte unchanged rather than
// rejected, replaced, or folded - such a byte's raw numeric value is
// never run through the fold table, even though it can coincidentally
// fall inside one of that table's ranges (see the .cpp). There is no
// explicit parse-error result or exception path for malformed input -
// this function always returns a string covering every input byte
// exactly once, either folded (valid, canonical UTF-8) or copied through
// unchanged (everything else). That is a statement about how malformed
// input is *handled*, not a claim that the function cannot throw at all
// - like any function returning a std::string, an allocation failure can
// still throw std::bad_alloc.
std::string normalize_search_text(std::string_view utf8);

} // namespace edopro_next::data

#endif // EDOPRO_NEXT_DATA_TEXT_NORMALIZE_H

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
// meant to be one. Malformed UTF-8 bytes are passed through unchanged
// rather than rejected: this function never fails and never throws.
std::string normalize_search_text(std::string_view utf8);

} // namespace edopro_next::data

#endif // EDOPRO_NEXT_DATA_TEXT_NORMALIZE_H

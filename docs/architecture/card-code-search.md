# Decimal card-code input

Batch 28 is an input adapter for the existing search box, not complete legacy
search parity. Source read at fork head `1068ee31` (upstream basis
`54ea755aa0243e2f18bb6bd2187fc9b2f7e29788`); no upstream files changed.

## Upstream source

`gframe/bufferio.h:240-248`, `BufferIO::GetVal`:

```cpp
static uint32_t GetVal(const wchar_t* pstr) {
    uint32_t ret = 0;
    while(*pstr >= L'0' && *pstr <= L'9') {
        ret = ret * 10 + (*pstr - L'0');
        pstr++;
    }
    if(*pstr == 0)
        return ret;
    return 0;
}
```

It accepts ASCII digits only, leading zeroes, and requires a terminating
NUL after the digits. It does not trim spaces or check overflow. Assignment
back into `uint32_t` wraps modulo 2^32; `4294967419` becomes 123.

`gframe/deck_con.cpp:1095-1107`, inside `FilterCards`:

```cpp
for(const auto& term_ : searchterms) {
    int trycode = BufferIO::GetVal(term_.data());
    const CardDataC* data = nullptr;
    if(trycode && (data = gDataManager->GetCardData(trycode))) {
        auto it = searched_terms.find(term_);
        if(it != searched_terms.end()) {
            it->second = { data };
        } else {
            searched_terms.emplace(std::wstring{ term_ }, std::vector{ data });
        }
        continue;
    }
```

`trycode` additionally converts unsigned to signed `int` (out-of-range
conversion was implementation-defined in upstream's C++17). A zero or
missing code falls through to ordinary subterm parsing. The shortcut's
`continue` skips the later candidate loop (`:1154-1157`):

```cpp
for(const auto& card : gDataManager->cards) {
    if(!CheckCardProperties(card.second))
        continue;
```

That bypass includes token/hidden/unofficial exclusions, type/stat/category
filters and banlist visibility, all in `CheckCardProperties`. Numeric lookup
is per `||` term upstream; our adapter does not implement that grammar.

## Adapter contract and deliberate differences

Trim outer whitespace using `QString::trimmed()`. Only a complete ASCII
decimal representing a nonzero uint32 **present in the current catalogue**
selects lookup mode. Accept arbitrarily many leading zeroes; reject overflow
before multiplication. Signs, internal whitespace, non-ASCII digits, zero,
overflow and unknown codes keep the original ordinary text query.

For a known code, clear only the query's text constraint, retain the entire
structured query, use `exact_code` as its existing ranking hint, and restrict
returned rows to the requested identity. Every candidate still passes
`policy::deck_search_admits`; failure of a filter yields no rows, never a
fallback to text. ADR 0005's data API is unchanged. Ordinary searches retain
ranking and the adapter's post-visibility 200-row cap.

Each refresh resolves existence again. Existing catalogue-load, filter,
banlist-load and selected-banlist signals therefore recompute both mode and
results. Clearing restores ordinary browsing. No QML or startup changes.

`ui/tests/test_card_code_search.cpp` uses disposable synthetic catalogues,
including a numeric text distractor and more than 200 browse matches.
Assertions distinguish lookup from text matching, reject wrap-to-known-code,
exercise filters/visibility, and transition unknown → known → removed on
reload. The tests failed against the pre-feature adapter. See batch evidence
for platform checks and retained failures; no duel-equivalence claim follows.

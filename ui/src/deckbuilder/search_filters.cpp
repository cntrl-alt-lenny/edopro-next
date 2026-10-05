// SPDX-License-Identifier: AGPL-3.0-or-later

#include "search_filters.h"

#include "edopro_next/data/numeric_filter_text.h"

namespace edopro_next::ui {

namespace {

// ocgcore/ocgapi_constants.h:33-58, and TYPE_SKILL from
// gframe/data_manager.h:36.
constexpr std::uint32_t TYPE_MONSTER = 0x1;
constexpr std::uint32_t TYPE_SPELL = 0x2;
constexpr std::uint32_t TYPE_TRAP = 0x4;
constexpr std::uint32_t TYPE_NORMAL = 0x10;
constexpr std::uint32_t TYPE_EFFECT = 0x20;
constexpr std::uint32_t TYPE_FUSION = 0x40;
constexpr std::uint32_t TYPE_RITUAL = 0x80;
constexpr std::uint32_t TYPE_SPIRIT = 0x200;
constexpr std::uint32_t TYPE_UNION = 0x400;
constexpr std::uint32_t TYPE_GEMINI = 0x800;
constexpr std::uint32_t TYPE_TUNER = 0x1000;
constexpr std::uint32_t TYPE_SYNCHRO = 0x2000;
constexpr std::uint32_t TYPE_MAXIMUM = 0x8000;
constexpr std::uint32_t TYPE_QUICKPLAY = 0x10000;
constexpr std::uint32_t TYPE_CONTINUOUS = 0x20000;
constexpr std::uint32_t TYPE_EQUIP = 0x40000;
constexpr std::uint32_t TYPE_FIELD = 0x80000;
constexpr std::uint32_t TYPE_COUNTER = 0x100000;
constexpr std::uint32_t TYPE_FLIP = 0x200000;
constexpr std::uint32_t TYPE_TOON = 0x400000;
constexpr std::uint32_t TYPE_XYZ = 0x800000;
constexpr std::uint32_t TYPE_PENDULUM = 0x1000000;
constexpr std::uint32_t TYPE_SPSUMMON = 0x2000000;
constexpr std::uint32_t TYPE_LINK = 0x4000000;
constexpr std::uint32_t TYPE_SKILL = 0x8000000;

// Upstream takes every label here from its string resource (strings.conf,
// read through DataManager::GetSysString), which this repository does not
// contain. These English labels are this project's; the values are
// upstream's (ADR 0012).
const QString kAny = QStringLiteral("Any");

} // namespace

QStringList cardTypeLabels() {
    // game.cpp:3351-3357: strings 1310, 1312, 1313, 1314, 1077.
    return {kAny, QStringLiteral("Monster"), QStringLiteral("Spell"), QStringLiteral("Trap"),
            QStringLiteral("Skill")};
}

const std::vector<LabelledValue>& subTypeChoices(CardTypeChoice cardType) {
    // game.cpp:3367-3371: All and Skill have one disabled "any" entry.
    static const std::vector<LabelledValue> none{{kAny, 0}};
    // game.cpp:3372-3393, in this order - index 8 is Link, which
    // deck_con.cpp:578 tests by position.
    static const std::vector<LabelledValue> monster{
        {kAny, 0},
        {QStringLiteral("Normal"), TYPE_MONSTER + TYPE_NORMAL},
        {QStringLiteral("Effect"), TYPE_MONSTER + TYPE_EFFECT},
        {QStringLiteral("Fusion"), TYPE_MONSTER + TYPE_FUSION},
        {QStringLiteral("Ritual"), TYPE_MONSTER + TYPE_RITUAL},
        {QStringLiteral("Synchro"), TYPE_MONSTER + TYPE_SYNCHRO},
        {QStringLiteral("Xyz"), TYPE_MONSTER + TYPE_XYZ},
        {QStringLiteral("Pendulum"), TYPE_MONSTER + TYPE_PENDULUM},
        {QStringLiteral("Link"), TYPE_MONSTER + TYPE_LINK},
        {QStringLiteral("Special Summon"), TYPE_MONSTER + TYPE_SPSUMMON},
        {QStringLiteral("Normal|Tuner"), TYPE_MONSTER + TYPE_NORMAL + TYPE_TUNER},
        {QStringLiteral("Normal|Pendulum"), TYPE_MONSTER + TYPE_NORMAL + TYPE_PENDULUM},
        {QStringLiteral("Synchro|Tuner"), TYPE_MONSTER + TYPE_SYNCHRO + TYPE_TUNER},
        {QStringLiteral("Tuner"), TYPE_MONSTER + TYPE_TUNER},
        {QStringLiteral("Gemini"), TYPE_MONSTER + TYPE_GEMINI},
        {QStringLiteral("Union"), TYPE_MONSTER + TYPE_UNION},
        {QStringLiteral("Spirit"), TYPE_MONSTER + TYPE_SPIRIT},
        {QStringLiteral("Flip"), TYPE_MONSTER + TYPE_FLIP},
        {QStringLiteral("Toon"), TYPE_MONSTER + TYPE_TOON},
        {QStringLiteral("Maximum"), TYPE_MONSTER + TYPE_MAXIMUM},
    };
    // game.cpp:3394-3403.
    static const std::vector<LabelledValue> spell{
        {kAny, 0},
        {QStringLiteral("Normal"), TYPE_SPELL},
        {QStringLiteral("Quick-Play"), TYPE_SPELL + TYPE_QUICKPLAY},
        {QStringLiteral("Continuous"), TYPE_SPELL + TYPE_CONTINUOUS},
        {QStringLiteral("Ritual"), TYPE_SPELL + TYPE_RITUAL},
        {QStringLiteral("Equip"), TYPE_SPELL + TYPE_EQUIP},
        {QStringLiteral("Field"), TYPE_SPELL + TYPE_FIELD},
        {QStringLiteral("Link"), TYPE_SPELL + TYPE_LINK},
    };
    // game.cpp:3404-3409.
    static const std::vector<LabelledValue> trap{
        {kAny, 0},
        {QStringLiteral("Normal"), TYPE_TRAP},
        {QStringLiteral("Continuous"), TYPE_TRAP + TYPE_CONTINUOUS},
        {QStringLiteral("Counter"), TYPE_TRAP + TYPE_COUNTER},
    };
    switch (cardType) {
    case CardTypeChoice::Monster:
        return monster;
    case CardTypeChoice::Spell:
        return spell;
    case CardTypeChoice::Trap:
        return trap;
    case CardTypeChoice::All:
    case CardTypeChoice::Skill:
        break;
    }
    return none;
}

const std::vector<LabelledValue>& attributeChoices() {
    // game.cpp:3443-3448: 0, then 0x1 << i up to ATTRIBUTE_DIVINE
    // (ocgcore/ocgapi_constants.h:61-67).
    static const std::vector<LabelledValue> choices{
        {kAny, 0},
        {QStringLiteral("EARTH"), 0x01},
        {QStringLiteral("WATER"), 0x02},
        {QStringLiteral("FIRE"), 0x04},
        {QStringLiteral("WIND"), 0x08},
        {QStringLiteral("LIGHT"), 0x10},
        {QStringLiteral("DARK"), 0x20},
        {QStringLiteral("DIVINE"), 0x40},
    };
    return choices;
}

const std::vector<LabelledValue>& raceChoices() {
    // game.cpp:3449-3462 offers bits 0-31 always and bits 32-63 when the
    // string resource names them; StartFilter turns item i + 1 into
    // `UINT64_C(1) << i` (deck_con.cpp:1046-1050). Without the resource,
    // this project offers the races ocgcore/ocgapi_constants.h:71-103 names:
    // bits 0-31 and RACE_YOKAI (bit 62) (ADR 0012).
    static const std::vector<LabelledValue> choices{
        {kAny, 0},
        {QStringLiteral("Warrior"), UINT64_C(1) << 0},
        {QStringLiteral("Spellcaster"), UINT64_C(1) << 1},
        {QStringLiteral("Fairy"), UINT64_C(1) << 2},
        {QStringLiteral("Fiend"), UINT64_C(1) << 3},
        {QStringLiteral("Zombie"), UINT64_C(1) << 4},
        {QStringLiteral("Machine"), UINT64_C(1) << 5},
        {QStringLiteral("Aqua"), UINT64_C(1) << 6},
        {QStringLiteral("Pyro"), UINT64_C(1) << 7},
        {QStringLiteral("Rock"), UINT64_C(1) << 8},
        {QStringLiteral("Winged Beast"), UINT64_C(1) << 9},
        {QStringLiteral("Plant"), UINT64_C(1) << 10},
        {QStringLiteral("Insect"), UINT64_C(1) << 11},
        {QStringLiteral("Thunder"), UINT64_C(1) << 12},
        {QStringLiteral("Dragon"), UINT64_C(1) << 13},
        {QStringLiteral("Beast"), UINT64_C(1) << 14},
        {QStringLiteral("Beast-Warrior"), UINT64_C(1) << 15},
        {QStringLiteral("Dinosaur"), UINT64_C(1) << 16},
        {QStringLiteral("Fish"), UINT64_C(1) << 17},
        {QStringLiteral("Sea Serpent"), UINT64_C(1) << 18},
        {QStringLiteral("Reptile"), UINT64_C(1) << 19},
        {QStringLiteral("Psychic"), UINT64_C(1) << 20},
        {QStringLiteral("Divine"), UINT64_C(1) << 21},
        {QStringLiteral("Creator God"), UINT64_C(1) << 22},
        {QStringLiteral("Wyrm"), UINT64_C(1) << 23},
        {QStringLiteral("Cyberse"), UINT64_C(1) << 24},
        {QStringLiteral("Illusion"), UINT64_C(1) << 25},
        {QStringLiteral("Cyborg"), UINT64_C(1) << 26},
        {QStringLiteral("Magical Knight"), UINT64_C(1) << 27},
        {QStringLiteral("High Dragon"), UINT64_C(1) << 28},
        {QStringLiteral("Omega Psychic"), UINT64_C(1) << 29},
        {QStringLiteral("Celestial Warrior"), UINT64_C(1) << 30},
        {QStringLiteral("Galaxy"), UINT64_C(1) << 31},
        {QStringLiteral("Yokai"), UINT64_C(1) << 62},
    };
    return choices;
}

QStringList categoryLabels() {
    // Upstream names these with strings 1100-1131 (game.cpp:722), which this
    // repository does not contain; this project shows the bit (ADR 0012).
    QStringList labels;
    for (int i = 0; i < 32; ++i)
        labels.push_back(QStringLiteral("Category %1 (0x%2)").arg(i + 1).arg(std::uint32_t{1} << i, 0, 16));
    return labels;
}

const std::array<LinkMarkerChoice, 8>& linkMarkerChoices() {
    // game.cpp:734-741 (glyphs) and deck_con.cpp:448-463 (bits, octal there).
    static const std::array<LinkMarkerChoice, 8> choices{{
        {QStringLiteral("↖"), 0100},
        {QStringLiteral("↑"), 0200},
        {QStringLiteral("↗"), 0400},
        {QStringLiteral("←"), 0010},
        {QStringLiteral("→"), 0040},
        {QStringLiteral("↙"), 0001},
        {QStringLiteral("↓"), 0002},
        {QStringLiteral("↘"), 0004},
    }};
    return choices;
}

QString limitationLabel(policy::LimitationFilter filter, bool whitelist) {
    using L = policy::LimitationFilter;
    switch (filter) {
    case L::None:
        return whitelist ? QStringLiteral("On the whitelist") : kAny;
    case L::Banned:
        return QStringLiteral("Banned");
    case L::Limited:
        return QStringLiteral("Limited");
    case L::SemiLimited:
        return QStringLiteral("Semi-limited");
    case L::Unlimited:
        return QStringLiteral("Unlimited");
    case L::Ocg:
        return QStringLiteral("OCG");
    case L::Tcg:
        return QStringLiteral("TCG");
    case L::TcgOcg:
        return QStringLiteral("OCG & TCG");
    case L::Prerelease:
        return QStringLiteral("Pre-release");
    case L::Speed:
        return QStringLiteral("Speed Duel");
    case L::Rush:
        return QStringLiteral("Rush Duel");
    case L::Legend:
        return QStringLiteral("Legend");
    case L::Anime:
        return QStringLiteral("Anime");
    case L::Illegal:
        return QStringLiteral("Illegal");
    case L::VideoGame:
        return QStringLiteral("Video game");
    case L::Custom:
        return QStringLiteral("Custom");
    case L::All:
        return QStringLiteral("All cards");
    }
    return QString();
}

bool monsterFiltersApply(const SearchFilterState& state) {
    return state.cardType == CardTypeChoice::Monster;
}

bool defenseFilterApplies(const SearchFilterState& state) {
    // deck_con.cpp:578: `cbCardType2->getSelected() == 8`, the Link entry.
    return monsterFiltersApply(state) && state.subType != 8;
}

data::SearchQuery buildSearchQuery(const SearchFilterState& state, const QString& text) {
    data::SearchQuery query;
    query.text = text.toStdString();

    const auto& subTypes = subTypeChoices(state.cardType);
    const std::uint32_t subType =
        (state.subType >= 0 && static_cast<std::size_t>(state.subType) < subTypes.size())
            ? static_cast<std::uint32_t>(subTypes[static_cast<std::size_t>(state.subType)].value)
            : 0;

    // CheckCardProperties's switch on the card type, deck_con.cpp:1194-1249.
    switch (state.cardType) {
    case CardTypeChoice::All:
        break;
    case CardTypeChoice::Monster: {
        // :1196: `!(type & TYPE_MONSTER) || (type & filter_type2) != filter_type2`.
        query.type = data::BitmaskFilter{TYPE_MONSTER | subType};
        // :1198-1201, with StartFilter's reading of the selectors (:1045-1050).
        const auto& attributes = attributeChoices();
        if (state.attribute > 0 && static_cast<std::size_t>(state.attribute) < attributes.size())
            query.attribute = static_cast<std::uint32_t>(attributes[static_cast<std::size_t>(state.attribute)].value);
        const auto& races = raceChoices();
        if (state.race > 0 && static_cast<std::size_t>(state.race) < races.size())
            query.race = races[static_cast<std::size_t>(state.race)].value;
        // :1051-1054 and :1202-1227.
        using data::NumericFilterField;
        query.attack = data::parse_numeric_filter(state.attackText.toStdString(), NumericFilterField::Attack);
        if (defenseFilterApplies(state))
            query.defense = data::parse_numeric_filter(state.defenseText.toStdString(), NumericFilterField::Defense);
        query.level = data::parse_numeric_filter(state.levelText.toStdString(), NumericFilterField::Level);
        query.left_scale = data::parse_numeric_filter(state.scaleText.toStdString(), NumericFilterField::Scale);
        break;
    }
    case CardTypeChoice::Spell:
    case CardTypeChoice::Trap: {
        // :1231-1234 and :1238-1241: the type bit, then, with a sub-type,
        // the whole type word.
        query.type = data::BitmaskFilter{state.cardType == CardTypeChoice::Spell ? TYPE_SPELL : TYPE_TRAP};
        if (subType)
            query.type_equals = subType;
        break;
    }
    case CardTypeChoice::Skill:
        // :1245-1246.
        query.type = data::BitmaskFilter{TYPE_SKILL};
        break;
    }
    // :1250-1253: effect categories (any of) and link markers (all of), for
    // every card type.
    if (state.categoryMask)
        query.category = data::AnyBitmaskFilter{state.categoryMask};
    if (state.linkMarkerMask)
        query.link_marker = data::BitmaskFilter{state.linkMarkerMask};
    return query;
}

} // namespace edopro_next::ui

# Builder report: 022-deck-builder-filters

Evidence below is at commit `da45ceeefc62ba731a1f0468b526f27d3980c08a` (the last code/docs
commit; `fw.py report` commits this file on top of it, which changes no code), on macOS, in a
fresh clone whose absolute path is written `<clone>` below.

## Verified

### Seat start (required evidence 1)

- `python3 tools/fw.py start --role builder --round 022-deck-builder-filters` → exit 0
  ```
  seat ok: builder, round 022-deck-builder-filters, branch builder/022-deck-builder-filters at 1065c21a7826
    brief: docs/rounds/022-deck-builder-filters/brief.md
    finish with: write docs/rounds/022-deck-builder-filters/builder.md, then python3 tools/fw.py report --role builder --round 022-deck-builder-filters --push
  ```
- `git submodule update --init` → exit 0
  ```
  Submodule 'ocgcore' (https://github.com/edo9300/ygopro-core.git) registered for path 'ocgcore'
  Cloning into '<clone>/ocgcore'...
  Submodule path 'ocgcore': checked out '46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57'
  ```
- `git submodule status` → exit 0
  ```
   46779fbe40e6a9bd8967f5dc6a03f4eaa6550d57 ocgcore (v11.0-86-g46779fb)
  ```
- Environment, each from a command: `sw_vers` → macOS 27.0 (build 26A428); `uname -m` → arm64;
  `c++ --version` → Apple clang version 21.0.0 (clang-2100.3.34.2); `cmake --version` → 4.4.3;
  `ninja --version` → 1.13.2; `sqlite3 --version` → 3.54.0; `qmake6 -query QT_VERSION` →
  **6.11.1**. **This differs from CI's pinned Qt 6.8.3** (`.github/workflows/edopro-next.yml:189`,
  `version: "6.8.3"`), which only CI ran. Python: system 3.9.6 and Homebrew 3.13.15.

### What was built

- **`data/`**: `data::parse_numeric_filter(text, field)` (`numeric_filter_text.h`) reproduces
  upstream's `parse_filter` + `BufferIO::GetVal` for the ATK, DEF, Level and Scale boxes;
  `NumericFilter` gains `excludes_negative`; `SearchQuery` gains `type_equals`.
- **`policy/`**: `deck_search_admits()`, `limitation_filter_choices()`,
  `non_official_switch_available()` (`deck_search_filter.h`): the hidden-card gate, the whole
  limit list (banlist counts, card-pool categories, whitelist), and which choices it offers.
  `limitation_for()` moved from `deck_validation.cpp` to `lf_list.h` so both use one copy.
- **`ui/`**: `search_filters.{h,cpp}` (upstream's option tables, and choices → `SearchQuery`),
  `SearchResultsModel` filter properties bound to the selected banlist through
  `deckController`, and the filter grid, effect-category popup and link-marker popup in
  `DeckBuilderScreen.qml`, which only sets indexes and text.

### The upstream passages relied on (required evidence 3)

Printed from the clone's `gframe/` by line range, not from memory; line numbers match the
citations in card-search.md §2.2-§2.3, ADR 0012 and the code comments.

**parse_filter** — `gframe/deck_con.cpp`
```
21: static int parse_filter(const wchar_t* pstr, uint32_t& type) {
22: 	if(*pstr == L'=') {
23: 		type = 1;
24: 		return BufferIO::GetVal(pstr + 1);
25: 	}
26: 	if(*pstr >= L'0' && *pstr <= L'9') {
27: 		type = 1;
28: 		return BufferIO::GetVal(pstr);
29: 	}
30: 	if(*pstr == L'>') {
31: 		if(*(pstr + 1) == L'=') {
32: 			type = 2;
33: 			return BufferIO::GetVal(pstr + 2);
34: 		}
35: 		type = 3;
36: 		return BufferIO::GetVal(pstr + 1);
37: 	}
38: 	if(*pstr == L'<') {
39: 		if(*(pstr + 1) == L'=') {
40: 			type = 4;
41: 			return BufferIO::GetVal(pstr + 2);
42: 		}
43: 		type = 5;
44: 		return BufferIO::GetVal(pstr + 1);
45: 	}
46: 	if(*pstr == L'?') {
47: 		type = 6;
48: 		return 0;
49: 	}
50: 	type = 0;
51: 	return 0;
```

**BufferIO::GetVal** — `gframe/bufferio.h`
```
240: 	static uint32_t GetVal(const wchar_t* pstr) {
241: 		uint32_t ret = 0;
242: 		while(*pstr >= L'0' && *pstr <= L'9') {
243: 			ret = ret * 10 + (*pstr - L'0');
244: 			pstr++;
245: 		}
246: 		if(*pstr == 0)
247: 			return ret;
248: 		return 0;
249: 	}
```

**limitation_search_filters and the filter_* widths** — `gframe/deck_con.h`
```
20: 	enum limitation_search_filters {
21: 		LIMITATION_FILTER_NONE,
22: 		LIMITATION_FILTER_BANNED,
23: 		LIMITATION_FILTER_LIMITED,
24: 		LIMITATION_FILTER_SEMI_LIMITED,
25: 		LIMITATION_FILTER_UNLIMITED,
26: 		LIMITATION_FILTER_OCG,
27: 		LIMITATION_FILTER_TCG,
28: 		LIMITATION_FILTER_TCG_OCG,
29: 		LIMITATION_FILTER_PRERELEASE,
30: 		LIMITATION_FILTER_SPEED,
31: 		LIMITATION_FILTER_RUSH,
32: 		LIMITATION_FILTER_LEGEND,
33: 		LIMITATION_FILTER_ANIME,
34: 		LIMITATION_FILTER_ILLEGAL,
35: 		LIMITATION_FILTER_VIDEOGAME,
36: 		LIMITATION_FILTER_CUSTOM,
37: 		LIMITATION_FILTER_ALL
38: 	};
...
110: 	DECLARE_WITH_CACHE(uint64_t, filter_effect)
111: 	DECLARE_WITH_CACHE(uint32_t, filter_type)
112: 	DECLARE_WITH_CACHE(uint32_t, filter_type2)
113: 	DECLARE_WITH_CACHE(uint32_t, filter_attrib)
114: 	DECLARE_WITH_CACHE(uint64_t, filter_race)
115: 	DECLARE_WITH_CACHE(uint32_t, filter_atktype)
116: 	DECLARE_WITH_CACHE(int32_t, filter_atk)
117: 	DECLARE_WITH_CACHE(uint32_t, filter_deftype)
118: 	DECLARE_WITH_CACHE(int32_t, filter_def)
119: 	DECLARE_WITH_CACHE(uint32_t, filter_lvtype)
120: 	DECLARE_WITH_CACHE(uint32_t, filter_lv)
121: 	DECLARE_WITH_CACHE(uint32_t, filter_scltype)
122: 	DECLARE_WITH_CACHE(uint32_t, filter_scl)
123: 	DECLARE_WITH_CACHE(uint32_t, filter_marks)
124: 	DECLARE_WITH_CACHE(limitation_search_filters, filter_lm)
```

**StartFilter reading the controls** — `gframe/deck_con.cpp`
```
1040: void DeckBuilder::StartFilter(bool force_refresh) {
1041: 	filter_type = mainGame->cbCardType->getItemData(mainGame->cbCardType->getSelected());
1042: 	filter_type2 = mainGame->cbCardType2->getItemData(mainGame->cbCardType2->getSelected());
1043: 	filter_lm = static_cast<limitation_search_filters>(mainGame->cbLimit->getItemData(mainGame->cbLimit->getSelected()));
1044: 	if(filter_type == CARD_TYPE_FILTER_MONSTER) {
1045: 		filter_attrib = mainGame->cbAttribute->getItemData(mainGame->cbAttribute->getSelected());
1046: 		auto selected = mainGame->cbRace->getItemData(mainGame->cbRace->getSelected());
1047: 		if(selected == 0)
1048: 			filter_race = 0;
1049: 		else
1050: 			filter_race = UINT64_C(1) << (selected - 1);
1051: 		filter_atk = parse_filter(mainGame->ebAttack->getText(), filter_atktype);
1052: 		filter_def = parse_filter(mainGame->ebDefense->getText(), filter_deftype);
1053: 		filter_lv = parse_filter(mainGame->ebStar->getText(), filter_lvtype);
1054: 		filter_scl = parse_filter(mainGame->ebScale->getText(), filter_scltype);
1055: 	}
```

**CheckCardProperties** — `gframe/deck_con.cpp`
```
1191: bool DeckBuilder::CheckCardProperties(const CardDataM& data) {
1192: 	if(data._data.type & TYPE_TOKEN || data._data.ot & SCOPE_HIDDEN || ((data._data.ot & SCOPE_OFFICIAL) != data._data.ot && (!mainGame->chkAnime->isChecked() && !filterList->whitelist)))
1193: 		return false;
1194: 	switch(filter_type) {
1195: 	case CARD_TYPE_FILTER_MONSTER: {
1196: 		if(!(data._data.type & TYPE_MONSTER) || (data._data.type & filter_type2) != filter_type2)
1197: 			return false;
1198: 		if(filter_race && data._data.race != filter_race)
1199: 			return false;
1200: 		if(filter_attrib && data._data.attribute != filter_attrib)
1201: 			return false;
1202: 		if(filter_atktype) {
1203: 			if((filter_atktype == 1 && data._data.attack != filter_atk) || (filter_atktype == 2 && data._data.attack < filter_atk)
1204: 				|| (filter_atktype == 3 && data._data.attack <= filter_atk) || (filter_atktype == 4 && (data._data.attack > filter_atk || data._data.attack < 0))
1205: 				|| (filter_atktype == 5 && (data._data.attack >= filter_atk || data._data.attack < 0)) || (filter_atktype == 6 && data._data.attack != -2))
1206: 				return false;
1207: 		}
1208: 		if(filter_deftype) {
1209: 			if((filter_deftype == 1 && data._data.defense != filter_def) || (filter_deftype == 2 && data._data.defense < filter_def)
1210: 				|| (filter_deftype == 3 && data._data.defense <= filter_def) || (filter_deftype == 4 && (data._data.defense > filter_def || data._data.defense < 0))
1211: 				|| (filter_deftype == 5 && (data._data.defense >= filter_def || data._data.defense < 0)) || (filter_deftype == 6 && data._data.defense != -2)
1212: 				|| (data._data.type & TYPE_LINK))
1213: 				return false;
1214: 		}
1215: 		if(filter_lvtype) {
1216: 			if((filter_lvtype == 1 && data._data.level != filter_lv) || (filter_lvtype == 2 && data._data.level < filter_lv)
1217: 				|| (filter_lvtype == 3 && data._data.level <= filter_lv) || (filter_lvtype == 4 && data._data.level > filter_lv)
1218: 				|| (filter_lvtype == 5 && data._data.level >= filter_lv) || filter_lvtype == 6)
1219: 				return false;
1220: 		}
1221: 		if(filter_scltype) {
1222: 			if((filter_scltype == 1 && data._data.lscale != filter_scl) || (filter_scltype == 2 && data._data.lscale < filter_scl)
1223: 				|| (filter_scltype == 3 && data._data.lscale <= filter_scl) || (filter_scltype == 4 && (data._data.lscale > filter_scl))
1224: 				|| (filter_scltype == 5 && (data._data.lscale >= filter_scl)) || filter_scltype == 6
1225: 				|| !(data._data.type & TYPE_PENDULUM))
1226: 				return false;
1227: 		}
1228: 		break;
1229: 	}
1230: 	case CARD_TYPE_FILTER_SPELL: {
1231: 		if(!(data._data.type & TYPE_SPELL))
1232: 			return false;
1233: 		if(filter_type2 && data._data.type != filter_type2)
1234: 			return false;
1235: 		break;
1236: 	}
1237: 	case CARD_TYPE_FILTER_TRAP: {
1238: 		if(!(data._data.type & TYPE_TRAP))
1239: 			return false;
1240: 		if(filter_type2 && data._data.type != filter_type2)
1241: 			return false;
1242: 		break;
1243: 	}
1244: 	case CARD_TYPE_FILTER_SKILL: {
1245: 		if(!(data._data.type & TYPE_SKILL))
1246: 			return false;
1247: 		break;
1248: 	}
1249: 	}
1250: 	if(filter_effect && !(data._data.category & filter_effect))
1251: 		return false;
1252: 	if(filter_marks && (data._data.link_marker & filter_marks) != filter_marks)
1253: 		return false;
1254: 	if((filter_lm != LIMITATION_FILTER_NONE || filterList->whitelist) && filter_lm != LIMITATION_FILTER_ALL) {
1255: 		auto flit = filterList->GetLimitationIterator(&data._data);
1256: 		int count = 3;
1257: 		if(flit == filterList->content.end()) {
1258: 			if(filterList->whitelist)
1259: 				count = -1;
1260: 		} else
1261: 			count = flit->second;
1262: 		switch(filter_lm) {
1263: 			case LIMITATION_FILTER_BANNED:
1264: 			case LIMITATION_FILTER_LIMITED:
1265: 			case LIMITATION_FILTER_SEMI_LIMITED:
1266: 				if(count != filter_lm - 1)
1267: 					return false;
1268: 				break;
1269: 			case LIMITATION_FILTER_UNLIMITED:
1270: 				if(count < 3)
1271: 					return false;
1272: 				break;
1273: 			case LIMITATION_FILTER_OCG:
1274: 				if(data._data.ot != SCOPE_OCG)
1275: 					return false;
1276: 				break;
1277: 			case LIMITATION_FILTER_TCG:
1278: 				if(data._data.ot != SCOPE_TCG)
1279: 					return false;
1280: 				break;
1281: 			case LIMITATION_FILTER_TCG_OCG:
1282: 				if(data._data.ot != SCOPE_OCG_TCG)
1283: 					return false;
1284: 				break;
1285: 			case LIMITATION_FILTER_PRERELEASE:
1286: 				if(!(data._data.ot & SCOPE_PRERELEASE))
1287: 					return false;
1288: 				break;
1289: 			case LIMITATION_FILTER_SPEED:
1290: 				if(!(data._data.ot & SCOPE_SPEED))
1291: 					return false;
1292: 				break;
1293: 			case LIMITATION_FILTER_RUSH:
1294: 				if(!(data._data.ot & SCOPE_RUSH))
1295: 					return false;
1296: 				break;
1297: 			case LIMITATION_FILTER_LEGEND:
1298: 				if(!(data._data.ot & SCOPE_LEGEND))
1299: 					return false;
1300: 				break;
1301: 			case LIMITATION_FILTER_ANIME:
1302: 				if(data._data.ot != SCOPE_ANIME)
1303: 					return false;
1304: 				break;
1305: 			case LIMITATION_FILTER_ILLEGAL:
1306: 				if(data._data.ot != SCOPE_ILLEGAL)
1307: 					return false;
1308: 				break;
1309: 			case LIMITATION_FILTER_VIDEOGAME:
1310: 				if(data._data.ot != SCOPE_VIDEO_GAME)
1311: 					return false;
1312: 				break;
1313: 			case LIMITATION_FILTER_CUSTOM:
1314: 				if(!(data._data.ot & SCOPE_CUSTOM))
1315: 					return false;
1316: 				break;
1317: 			default:
1318: 				break;
1319: 		}
1320: 		if(filterList->whitelist && count < 0)
1321: 			return false;
1322: 	}
1323: 	return true;
1324: }
```

**category OK, marker OK, card-type change, sub-type change (DEF), anime switch** — `gframe/deck_con.cpp`
```
346: 			case BUTTON_CATEGORY_OK: {
347: 				filter_effect = 0;
348: 				long long filter = 0x1;
349: 				for(int i = 0; i < 32; ++i, filter <<= 1)
350: 					if(mainGame->chkCategory[i]->isChecked())
351: 						filter_effect |= filter;
352: 				mainGame->HideElement(mainGame->wCategories);
353: 				break;
...
447: 				filter_marks = 0;
448: 				if (mainGame->btnMark[0]->isPressed())
449: 					filter_marks |= 0100;
450: 				if (mainGame->btnMark[1]->isPressed())
451: 					filter_marks |= 0200;
452: 				if (mainGame->btnMark[2]->isPressed())
453: 					filter_marks |= 0400;
454: 				if (mainGame->btnMark[3]->isPressed())
455: 					filter_marks |= 0010;
456: 				if (mainGame->btnMark[4]->isPressed())
457: 					filter_marks |= 0040;
458: 				if (mainGame->btnMark[5]->isPressed())
459: 					filter_marks |= 0001;
460: 				if (mainGame->btnMark[6]->isPressed())
461: 					filter_marks |= 0002;
462: 				if (mainGame->btnMark[7]->isPressed())
463: 					filter_marks |= 0004;
...
525: 			case COMBOBOX_MAINTYPE: {
526: 				mainGame->ReloadCBCardType2();
527: 				mainGame->cbAttribute->setSelected(0);
528: 				mainGame->cbRace->setSelected(0);
529: 				mainGame->ebAttack->setText(L"");
530: 				mainGame->ebDefense->setText(L"");
531: 				mainGame->ebStar->setText(L"");
532: 				mainGame->ebScale->setText(L"");
533: 				switch(mainGame->cbCardType->getItemData(mainGame->cbCardType->getSelected())) {
...
575: 			case COMBOBOX_SECONDTYPE:
576: 			case COMBOBOX_OTHER_FILT: {
577: 				if (id==COMBOBOX_SECONDTYPE && mainGame->cbCardType->getItemData(mainGame->cbCardType->getSelected()) == CARD_TYPE_FILTER_MONSTER) {
578: 					if (mainGame->cbCardType2->getSelected() == 8) {
579: 						mainGame->ebDefense->setEnabled(false);
580: 						mainGame->ebDefense->setText(L"");
581: 					} else {
582: 						mainGame->ebDefense->setEnabled(true);
583: 					}
584: 				}
585: 				StartFilter(true);
586: 				break;
587: 			}
...
607: 				case CHECKBOX_SHOW_ANIME: {
608: 					int prevLimit = mainGame->cbLimit->getSelected();
609: 					mainGame->ReloadCBLimit();
610: 					if (prevLimit < 8)
611: 						mainGame->cbLimit->setSelected(prevLimit);
612: 					StartFilter(true);
613: 					break;
614: 				}
```

**ClearSearch / ClearFilter** — `gframe/deck_con.cpp`
```
1363: void DeckBuilder::ClearSearch() {
1364: 	mainGame->cbCardType->setSelected(0);
1365: 	mainGame->cbCardType2->setSelected(0);
1366: 	mainGame->cbCardType2->setEnabled(false);
1367: 	mainGame->cbRace->setEnabled(false);
1368: 	mainGame->cbAttribute->setEnabled(false);
1369: 	mainGame->ebAttack->setEnabled(false);
1370: 	mainGame->ebDefense->setEnabled(false);
1371: 	mainGame->ebStar->setEnabled(false);
1372: 	mainGame->ebScale->setEnabled(false);
1373: 	mainGame->ebCardName->setText(L"");
1374: 	mainGame->scrFilter->setVisible(false);
1375: 	searched_terms.clear();
1376: 	ClearFilter();
1377: 	results.clear();
1378: 	result_string = L"0";
1379: 	scroll_pos = 0;
1380: 	mainGame->env->setFocus(mainGame->ebCardName);
1381: }
1382: void DeckBuilder::ClearFilter() {
1383: 	mainGame->cbAttribute->setSelected(0);
1384: 	mainGame->cbRace->setSelected(0);
1385: 	mainGame->cbLimit->setSelected(0);
1386: 	mainGame->ebAttack->setText(L"");
1387: 	mainGame->ebDefense->setText(L"");
1388: 	mainGame->ebStar->setText(L"");
1389: 	mainGame->ebScale->setText(L"");
1390: 	filter_effect = 0;
1391: 	for(int i = 0; i < 32; ++i)
1392: 		mainGame->chkCategory[i]->setChecked(false);
1393: 	filter_marks = 0;
1394: 	for(int i = 0; i < 8; i++)
1395: 		mainGame->btnMark[i]->setPressed(false);
1396: }
1397: void DeckBuilder::SortList() {
```

**sub-type, limit, attribute and race lists** — `gframe/game.cpp`
```
3363: void Game::ReloadCBCardType2() {
3364: 	cbCardType2->clear();
3365: 	cbCardType2->setEnabled(true);
3366: 	switch (cbCardType->getItemData(cbCardType->getSelected())) {
3367: 	case DeckBuilder::CARD_TYPE_FILTER_ALL:
3368: 	case DeckBuilder::CARD_TYPE_FILTER_SKILL:
3369: 		cbCardType2->setEnabled(false);
3370: 		cbCardType2->addItem(gDataManager->GetSysString(1310).data(), 0);
3371: 		break;
3372: 	case DeckBuilder::CARD_TYPE_FILTER_MONSTER:
3373: 		cbCardType2->addItem(gDataManager->GetSysString(1080).data(), 0);
3374: 		cbCardType2->addItem(gDataManager->GetSysString(1054).data(), TYPE_MONSTER + TYPE_NORMAL);
3375: 		cbCardType2->addItem(gDataManager->GetSysString(1055).data(), TYPE_MONSTER + TYPE_EFFECT);
3376: 		cbCardType2->addItem(gDataManager->GetSysString(1056).data(), TYPE_MONSTER + TYPE_FUSION);
3377: 		cbCardType2->addItem(gDataManager->GetSysString(1057).data(), TYPE_MONSTER + TYPE_RITUAL);
3378: 		cbCardType2->addItem(gDataManager->GetSysString(1063).data(), TYPE_MONSTER + TYPE_SYNCHRO);
3379: 		cbCardType2->addItem(gDataManager->GetSysString(1073).data(), TYPE_MONSTER + TYPE_XYZ);
3380: 		cbCardType2->addItem(gDataManager->GetSysString(1074).data(), TYPE_MONSTER + TYPE_PENDULUM);
3381: 		cbCardType2->addItem(gDataManager->GetSysString(1076).data(), TYPE_MONSTER + TYPE_LINK);
3382: 		cbCardType2->addItem(gDataManager->GetSysString(1075).data(), TYPE_MONSTER + TYPE_SPSUMMON);
3383: 		cbCardType2->addItem(epro::format(L"{}|{}", gDataManager->GetSysString(1054), gDataManager->GetSysString(1062)).data(), TYPE_MONSTER + TYPE_NORMAL + TYPE_TUNER);
3384: 		cbCardType2->addItem(epro::format(L"{}|{}", gDataManager->GetSysString(1054), gDataManager->GetSysString(1074)).data(), TYPE_MONSTER + TYPE_NORMAL + TYPE_PENDULUM);
3385: 		cbCardType2->addItem(epro::format(L"{}|{}", gDataManager->GetSysString(1063), gDataManager->GetSysString(1062)).data(), TYPE_MONSTER + TYPE_SYNCHRO + TYPE_TUNER);
3386: 		cbCardType2->addItem(gDataManager->GetSysString(1062).data(), TYPE_MONSTER + TYPE_TUNER);
3387: 		cbCardType2->addItem(gDataManager->GetSysString(1061).data(), TYPE_MONSTER + TYPE_GEMINI);
3388: 		cbCardType2->addItem(gDataManager->GetSysString(1060).data(), TYPE_MONSTER + TYPE_UNION);
3389: 		cbCardType2->addItem(gDataManager->GetSysString(1059).data(), TYPE_MONSTER + TYPE_SPIRIT);
3390: 		cbCardType2->addItem(gDataManager->GetSysString(1071).data(), TYPE_MONSTER + TYPE_FLIP);
3391: 		cbCardType2->addItem(gDataManager->GetSysString(1072).data(), TYPE_MONSTER + TYPE_TOON);
3392: 		cbCardType2->addItem(gDataManager->GetSysString(1065).data(), TYPE_MONSTER + TYPE_MAXIMUM);
3393: 		break;
3394: 	case DeckBuilder::CARD_TYPE_FILTER_SPELL:
3395: 		cbCardType2->addItem(gDataManager->GetSysString(1080).data(), 0);
3396: 		cbCardType2->addItem(gDataManager->GetSysString(1054).data(), TYPE_SPELL);
3397: 		cbCardType2->addItem(gDataManager->GetSysString(1066).data(), TYPE_SPELL + TYPE_QUICKPLAY);
3398: 		cbCardType2->addItem(gDataManager->GetSysString(1067).data(), TYPE_SPELL + TYPE_CONTINUOUS);
3399: 		cbCardType2->addItem(gDataManager->GetSysString(1057).data(), TYPE_SPELL + TYPE_RITUAL);
3400: 		cbCardType2->addItem(gDataManager->GetSysString(1068).data(), TYPE_SPELL + TYPE_EQUIP);
3401: 		cbCardType2->addItem(gDataManager->GetSysString(1069).data(), TYPE_SPELL + TYPE_FIELD);
3402: 		cbCardType2->addItem(gDataManager->GetSysString(1076).data(), TYPE_SPELL + TYPE_LINK);
3403: 		break;
3404: 	case DeckBuilder::CARD_TYPE_FILTER_TRAP:
3405: 		cbCardType2->addItem(gDataManager->GetSysString(1080).data(), 0);
3406: 		cbCardType2->addItem(gDataManager->GetSysString(1054).data(), TYPE_TRAP);
3407: 		cbCardType2->addItem(gDataManager->GetSysString(1067).data(), TYPE_TRAP + TYPE_CONTINUOUS);
3408: 		cbCardType2->addItem(gDataManager->GetSysString(1070).data(), TYPE_TRAP + TYPE_COUNTER);
3409: 		break;
3410: 	}
3411: }
3412: void Game::ReloadCBLimit() {
3413: 	bool white = deckBuilder.filterList && deckBuilder.filterList->whitelist;
3414: 	cbLimit->clear();
3415: 	cbLimit->addItem(gDataManager->GetSysString(white ? 1269 : 1310).data(), DeckBuilder::LIMITATION_FILTER_NONE);
3416: 	cbLimit->addItem(gDataManager->GetSysString(1316).data(), DeckBuilder::LIMITATION_FILTER_BANNED);
3417: 	cbLimit->addItem(gDataManager->GetSysString(1317).data(), DeckBuilder::LIMITATION_FILTER_LIMITED);
3418: 	cbLimit->addItem(gDataManager->GetSysString(1318).data(), DeckBuilder::LIMITATION_FILTER_SEMI_LIMITED);
3419: 	cbLimit->addItem(gDataManager->GetSysString(1320).data(), DeckBuilder::LIMITATION_FILTER_UNLIMITED);
3420: 	if(!white) {
3421: 		chkAnime->setEnabled(true);
3422: 		cbLimit->addItem(gDataManager->GetSysString(1900).data(), DeckBuilder::LIMITATION_FILTER_OCG);
3423: 		cbLimit->addItem(gDataManager->GetSysString(1901).data(), DeckBuilder::LIMITATION_FILTER_TCG);
3424: 		cbLimit->addItem(gDataManager->GetSysString(1902).data(), DeckBuilder::LIMITATION_FILTER_TCG_OCG);
3425: 		cbLimit->addItem(gDataManager->GetSysString(1903).data(), DeckBuilder::LIMITATION_FILTER_PRERELEASE);
3426: 		cbLimit->addItem(gDataManager->GetSysString(1910).data(), DeckBuilder::LIMITATION_FILTER_SPEED);
3427: 		cbLimit->addItem(gDataManager->GetSysString(1911).data(), DeckBuilder::LIMITATION_FILTER_RUSH);
3428: 		cbLimit->addItem(gDataManager->GetSysString(1912).data(), DeckBuilder::LIMITATION_FILTER_LEGEND);
3429: 		if(chkAnime->isChecked()) {
3430: 			cbLimit->addItem(gDataManager->GetSysString(1265).data(), DeckBuilder::LIMITATION_FILTER_ANIME);
3431: 			cbLimit->addItem(gDataManager->GetSysString(1266).data(), DeckBuilder::LIMITATION_FILTER_ILLEGAL);
3432: 			cbLimit->addItem(gDataManager->GetSysString(1267).data(), DeckBuilder::LIMITATION_FILTER_VIDEOGAME);
3433: 			cbLimit->addItem(gDataManager->GetSysString(1268).data(), DeckBuilder::LIMITATION_FILTER_CUSTOM);
3434: 		}
3435: 	} else {
3436: 		chkAnime->setEnabled(false);
3437: 		cbLimit->addItem(gDataManager->GetSysString(1912).data(), DeckBuilder::LIMITATION_FILTER_LEGEND);
3438: 		cbLimit->addItem(gDataManager->GetSysString(1266).data(), DeckBuilder::LIMITATION_FILTER_ILLEGAL);
3439: 		cbLimit->addItem(gDataManager->GetSysString(1903).data(), DeckBuilder::LIMITATION_FILTER_PRERELEASE);
3440: 		cbLimit->addItem(gDataManager->GetSysString(1310).data(), DeckBuilder::LIMITATION_FILTER_ALL);
3441: 	}
3442: }
3443: void Game::ReloadCBAttribute() {
3444: 	cbAttribute->clear();
3445: 	cbAttribute->addItem(gDataManager->GetSysString(1310).data(), 0);
3446: 	for (uint32_t filter = 0x1, i = 1010; filter <= ATTRIBUTE_DIVINE; filter <<= 1, i++)
3447: 		cbAttribute->addItem(gDataManager->GetSysString(i).data(), filter);
3448: }
3449: void Game::ReloadCBRace() {
3450: 	cbRace->clear();
3451: 	cbRace->addItem(gDataManager->GetSysString(1310).data(), 0);
3452: 	//currently corresponding to RACE_GALAXY
3453: 	static constexpr auto CURRENTLY_KNOWN_RACES = 32;
3454: 	uint32_t i = 0;
3455: 	for(; i < CURRENTLY_KNOWN_RACES; ++i)
3456: 		cbRace->addItem(gDataManager->GetSysString(gDataManager->GetRaceStringIndex(i)).data(), i + 1);
3457: 	for(; i < 64; ++i) {
3458: 		auto idx = gDataManager->GetRaceStringIndex(i);
3459: 		if(gDataManager->HasSysString(idx))
3460: 			cbRace->addItem(gDataManager->GetSysString(idx).data(), i + 1);
3461: 	}
3462: }
```

**card-type list and the non-official switch** — `gframe/game.cpp`
```
3350: void Game::ReloadCBCardType() {
3351: 	static constexpr std::array<std::pair<uint32_t, DeckBuilder::CARD_TYPE_FILTER>, 5> items{ {
3352: 		{1310, DeckBuilder::CARD_TYPE_FILTER_ALL},
3353: 		{1312, DeckBuilder::CARD_TYPE_FILTER_MONSTER},
3354: 		{1313, DeckBuilder::CARD_TYPE_FILTER_SPELL},
3355: 		{1314, DeckBuilder::CARD_TYPE_FILTER_TRAP},
3356: 		{1077, DeckBuilder::CARD_TYPE_FILTER_SKILL},
3357: 	} };
...
671: 	chkAnime = AlignElementWithParent(env->addCheckBox(gGameConfig->chkAnime, Scale(10, 96, 150, 118), wFilter, CHECKBOX_SHOW_ANIME, gDataManager->GetSysString(1999).data()));
```

**switch default** — `gframe/game_config.inl`
```
73: OPTION_ALIASED(bool, chkAnime, show_unofficial, false)
```

**LFList::GetLimitationIterator** — `gframe/deck_manager.h`
```
23: 	auto GetLimitationIterator(const CardDataC* pcard) const {
24: 		auto flit = content.find(pcard->code);
25: 		if(flit == content.end() && pcard->alias) {
26: 			if(!whitelist || pcard->IsInArtworkOffsetRange())
27: 				flit = content.find(pcard->alias);
28: 		}
29: 		return flit;
30: 	}
```

**SCOPE_* / TYPE_SKILL and IsInArtworkOffsetRange** — `gframe/data_manager.h`
```
21: #define SCOPE_OCG        0x1
22: #define SCOPE_TCG        0x2
23: #define SCOPE_ANIME      0x4
24: #define SCOPE_ILLEGAL    0x8
25: #define SCOPE_VIDEO_GAME 0x10
26: #define SCOPE_CUSTOM     0x20
27: #define SCOPE_SPEED      0x40
28: #define SCOPE_PRERELEASE 0x100
29: #define SCOPE_RUSH       0x200
30: #define SCOPE_LEGEND     0x400
31: #define SCOPE_HIDDEN     0x1000
32: 
33: #define SCOPE_OCG_TCG    (SCOPE_OCG | SCOPE_TCG)
34: #define SCOPE_OFFICIAL   (SCOPE_OCG | SCOPE_TCG | SCOPE_PRERELEASE)
35: 
36: #define TYPE_SKILL       0x8000000
...
76: 	bool IsInArtworkOffsetRange() const {
77: 		return IsInArtworkOffsetRange(this);
78: 	}
79: 
80: 	template<typename T>
81: 	static bool IsInArtworkOffsetRange(const T* pcard) {
82: 		if(pcard->alias == 0)
83: 			return false;
84: 		return (pcard->alias - pcard->code < CARD_ARTWORK_VERSIONS_OFFSET || pcard->code - pcard->alias < CARD_ARTWORK_VERSIONS_OFFSET);
85: 	}
```

**the card decode the comparisons read** — `gframe/data_manager.cpp`
```
137: 		cd.type = static_cast<uint32_t>(sqlite3_column_int64(pStmt, 4));
138: 		cd.attack = sqlite3_column_int(pStmt, 5);
139: 		cd.defense = sqlite3_column_int(pStmt, 6);
140: 		if(cd.type & TYPE_LINK) {
141: 			cd.link_marker = cd.defense;
142: 			cd.defense = 0;
143: 		} else
144: 			cd.link_marker = 0;
145: 
146: 		int level = sqlite3_column_int(pStmt, 7);
147: 		if(level < 0)
148: 			cd.level = -(level & 0xff);
149: 		else
150: 			cd.level = level & 0xff;
151: 
152: 		cd.lscale = (level >> 24) & 0xff;
153: 		cd.rscale = (level >> 16) & 0xff;
```


### Part 2: `NumericFilter` against every upstream input form (acceptance 2)

Finding: card-search.md §2.1's "deliberate simplification" was **not** true. Against the quoted
upstream: "at most" (`<=`, `<`) on ATK/DEF drops "?" stats (`deck_con.cpp:1204-1205`,
`:1210-1211`) and `AtMost` kept them; "?" is ATK/DEF `== -2` only, and matches nothing on
Level/Scale (`:1205`, `:1211`, `:1218`, `:1224`); trailing text after a recognised prefix makes
the number 0 (`bufferio.h:246-248`). I extended `data/` (ADR 0012, Decision 1) rather than
record divergences. `data/tests/test_numeric_filter_text.cpp`,
`every_numeric_input_form_keeps_exactly_the_cards_upstream_keeps`, compares, for 184 inputs
(9 prefixes × 19 numbers, plus 13 oddities such as `" 5"`, `"-5"`, `"=>5"`, `"??"`, an
Arabic-Indic digit, `">==5"`) on each of the four boxes, the cards this project keeps with the
cards a transcription of `parse_filter`, `GetVal`, the `:1202-1227` comparisons and upstream's
card decode (`data_manager.cpp:137-153`) keeps, over synthetic cards covering "?" stats,
`INT32_MAX`, Link and non-Link DEF, a negative level column, and scales on Pendulum and
non-Pendulum monsters. It also requires more than half the inputs to actually filter. Passes
(below). card-search.md §2.1 rows for `type`, `attack`/`defense`, `level`, scale and `scope`
were rewritten; §2.2 and §2.3 are new; §4 and §9 corrected.

### Acceptance criterion 5: tests fail against a wrong comparison and a wrong parser (required evidence 4)

Each change applied to a clean tree at `da45ceee`'s code, the named targets rebuilt and run,
then restored with `git checkout -- <file>`; `git status --short` was empty afterwards and all
tests passed again. Output shortened; the clone path is removed.

**Wrong comparison**

1. C1, `data/src/card_search_index.cpp`, `AtLeast` made strict:
   ```
   -		return field >= filter.value;
   +		return field > filter.value;
   ```
   `test_numeric_filter_text` → exit 1, `2 tests, 1 failed, 163 assertions failed`:
   ```
   FAIL every_numeric_input_form_keeps_exactly_the_cards_upstream_keeps
       field 0 input ">" differs: upstream-only:4
       field 0 input ">0" differs: upstream-only:4
   ```
   `test_deckbuilder` → exit 1, `Totals: 38 passed, 1 failed`:
   ```
   FAIL!  : TestDeckBuilder::searchFiltersDriveEveryUpstreamControl() Compared lists have different sizes.
      Actual   (resultCodes(model)) size: 4
      Expected ((L{101, 102, 103, 106, 107})) size: 5
   ```
2. C2, same file, `excludes_negative` ignored:
   ```
   -	if(filter.excludes_negative && field < 0)
   -		return false;
   ```
   `test_numeric_filter_text` → exit 1, `2 tests, 1 failed, 70 assertions failed`:
   ```
       field 0 input "<" differs: ours-only:1 ours-only:2
       field 0 input "<1" differs: ours-only:1 ours-only:2
   ```
   `test_deckbuilder` → exit 1: `Actual (resultCodes(model)) size: 3` / `Expected ((L{101, 104})) size: 2`.
3. L1 (a wrong banlist comparison), `policy/src/deck_search_filter.cpp`:
   ```
   -		if(count < 3)
   +		if(count < 2)
   ```
   `test_deck_search_filter` → exit 1, `2 tests, 1 failed, 3080 assertions failed`:
   ```
       code 102 ot 0x0 type 1 filter 4 anime 0 list black: upstream 0, ours 1
   ```
   (the "0x" label printed a decimal value; fixed afterwards in `4341b2b4`, message only).
   `test_deckbuilder` → exit 1:
   `FAIL!  : TestDeckBuilder::limitFilterFollowsTheSelectedBanlist()` /
   `Actual (resultCodes(model)) size: 3` / `Expected ((L{304, 305})) size: 2`.

**Wrong input parser**

4. P1, `data/src/numeric_filter_text.cpp`, strict `>` read as `>=`:
   ```
   -		return NumericFilter{n + 1, NumericComparison::AtLeast, false};
   +		return NumericFilter{n, NumericComparison::AtLeast, false};
   ```
   `test_numeric_filter_text` → exit 1, `2 tests, 2 failed, 124 assertions failed`
   (`field 0 input ">1" differs: ours-only:4`); `test_deckbuilder` → exit 1:
   `Actual (resultCodes(model)) size: 5` / `Expected ((L{102, 103, 106, 107})) size: 4`.
5. P2, same file, `GetVal` no longer zeroes a number with trailing text:
   ```
   -	if(pos == text.size())
   -		return ret;
   -	return 0;
   +	return ret;
   ```
   `test_numeric_filter_text` → exit 1, `2 tests, 2 failed, 29 assertions failed`
   (`field 0 input ">1000a" differs: upstream-only:4 upstream-only:5 upstream-only:6`);
   `test_deckbuilder` → exit 1: `Actual (resultCodes(model)) size: 1` / `Expected (L{}) size: 0`
   (the `"1500a"` case).

### Module cycles (required evidence 2), fresh `rm -rf data/build policy/build ui/build`, at `da45ceee`

```
da45ceeefc62ba731a1f0468b526f27d3980c08a
$ cmake -S data -B data/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DEDOPRO_NEXT_WERROR=ON
-- Generating done (0.0s)
-- Build files have been written to: <clone>/data/build
-> exit 0

$ cmake --build data/build
[23/24] Building CXX object tests/bench/CMakeFiles/bench_card_search.dir/__/bench_card_search.cpp.o
[24/24] Linking CXX executable tests/bench/bench_card_search
-> exit 0

data build warning lines: 0
$ ctest --test-dir data/build --output-on-failure
Test project <clone>/data/build
    Start 1: card_database
1/4 Test #1: card_database ....................   Passed    0.43 sec
    Start 2: deck_ydk
2/4 Test #2: deck_ydk .........................   Passed    0.16 sec
    Start 3: card_search
3/4 Test #3: card_search ......................   Passed    0.29 sec
    Start 4: numeric_filter_text
4/4 Test #4: numeric_filter_text ..............   Passed    0.23 sec

100% tests passed out of 4

Total Test time (real) =   1.12 sec
-> exit 0

$ cmake -S policy -B policy/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DEDOPRO_NEXT_WERROR=ON
-- Generating done (0.0s)
-- Build files have been written to: <clone>/policy/build
-> exit 0

$ cmake --build policy/build
[22/23] Building CXX object _data/CMakeFiles/edopro_next_search.dir/src/card_search_index.cpp.o
[23/23] Linking CXX static library _data/libedopro_next_search.a
-> exit 0

policy build warning lines: 0
$ ctest --test-dir policy/build --output-on-failure
Test project <clone>/policy/build
    Start 1: lf_list
1/4 Test #1: lf_list ..........................   Passed    0.18 sec
    Start 2: deck_validation
2/4 Test #2: deck_validation ..................   Passed    0.30 sec
    Start 3: deck_placement
3/4 Test #3: deck_placement ...................   Passed    0.20 sec
    Start 4: deck_search_filter
4/4 Test #4: deck_search_filter ...............   Passed    0.36 sec

100% tests passed out of 4

Total Test time (real) =   1.04 sec
-> exit 0

$ cmake -S ui -B ui/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DEDOPRO_NEXT_WERROR=ON -DEDOPRO_NEXT_UI_TESTS=ON
-- Generating done (0.1s)
-- Build files have been written to: <clone>/ui/build
-> exit 0

$ cmake --build ui/build
[103/104] Building CXX object tests/CMakeFiles/test_deckbuilder_screen.dir/test_deckbuilder_screen_qmltyperegistrations.cpp.o
[104/104] Linking CXX executable tests/test_deckbuilder_screen
ld: warning: ignoring duplicate libraries: 'data/libedopro_next_data.a', 'data/libedopro_next_deck.a'
-> exit 0

ui build:    2 ld: warning: ignoring duplicate libraries: 'data/libedopro_next_data.a', 'data/libedopro_next_deck.a'
$ ctest --test-dir ui/build --output-on-failure
Test project <clone>/ui/build
    Start 1: deckbuilder
1/2 Test #1: deckbuilder ......................   Passed    0.42 sec
    Start 2: deckbuilder_screen
2/2 Test #2: deckbuilder_screen ...............   Passed    1.63 sec

100% tests passed out of 2

Total Test time (real) =   2.06 sec
-> exit 0

$ ./ui/build/tests/test_deckbuilder
PASS   : TestDeckBuilder::cleanupTestCase()
Totals: 39 passed, 0 failed, 0 skipped, 0 blacklisted, 192ms
********* Finished testing of TestDeckBuilder *********
-> exit 0

$ ./ui/build/tests/test_deckbuilder_screen
PASS   : TestDeckBuilderScreen::cleanupTestCase()
Totals: 23 passed, 0 failed, 0 skipped, 0 blacklisted, 1098ms
********* Finished testing of TestDeckBuilderScreen *********
-> exit 0
```

- Offscreen clean-QML-load check (CI's step; macOS has no `timeout(1)`, so a Python wrapper
  starts `QT_QPA_PLATFORM=offscreen ./ui/build/edopro_next_shell`, waits 20 s, requires it to
  be running, kills it, reads stderr): **still running after 20 s**; **stderr not empty**, 148
  bytes, one Qt font notice, not a QML diagnostic: `qt.qpa.fonts: Populating font family
  aliases took 78 ms. Replace uses of missing font family "Sans Serif" with one that exists to
  avoid this cost.` `grep -ciE "qml|TypeError|ReferenceError"` on it → `0`. The same
  macOS-only notice as rounds 019-021. CI's Linux job runs the check verbatim (below).
- QML warnings in the screen test: with the first version of the non-official check box (a
  custom `contentItem`) `test_deckbuilder_screen` printed "The current style does not support
  customization of this control" (the test harness uses the native macOS style, the app uses
  Basic). I replaced it with `palette.windowText`; the test now prints no QML warning. The one
  remaining `QWARN` in `test_deckbuilder` ("Trying to construct an instance of an invalid type,
  type id: 4097", in `modelInvariantsHoldAcrossEveryMutation`) also occurs at `1065c21a`, the
  round's start commit (built and run separately), so it is not from this round.

### Search benchmark (required evidence 5)

The search path changed (`passes_numeric` gains the `excludes_negative` test, `search()` gains
the `type_equals` test). `bench_card_search`, Release, same machine, built before any change
(at `1065c21a`) and after (at `4341b2b4`, whose `data/` is `da45ceee`'s), run alternately three
times each (ms/query; 22,000 synthetic cards):

| Query | Before (3 runs) | After (3 runs) |
|---|---|---|
| exact-name | 4.02, 4.03, 4.22 | 4.06, 4.07, 4.15 |
| name-prefix | 3.89, 3.94, 4.07 | 3.94, 3.99, 4.02 |
| broad text-scan | 3.27, 3.37, 3.53 | 3.23, 3.24, 3.34 |
| filtered (type+atk+level), no text | 1.21, 1.21, 1.24 | 1.21, 1.22, 1.25 |
| ranked broad name | 3.01, 3.02, 3.09 | 2.96, 2.98, 4.58 |

No change beyond run-to-run noise (the 4.58 is one run). The deck builder's own path adds, per
result, a `CardDatabase::find` (a `std::map` lookup) and `policy::deck_search_admits`; that
path is **not** in the benchmark and was not measured.

### Python suite, generators, `fw.py check` (required evidence 6), at `da45ceee`

- Homebrew Python 3.13.15: `python3.13 -m unittest discover -s tests -v` → exit 0, `Ran 133
  tests`, `OK (skipped=11)`. Skips: `test_unreadable_source_tree_fails_closed` ("Windows ACL
  denial is required for this enumeration test"), and ten semantic-trace tests
  (`test_fixtures_exist`, `test_rendering_is_deterministic`, `test_traces_match_golden`,
  `test_committed_fixtures_are_semantically_complete`, `test_coverage_accounts_for_every_packet`,
  `test_model_invariants_hold_at_the_end`, `test_no_environmental_leakage`,
  `test_no_packet_is_malformed_or_unknown`, `test_query_stream_coverage_is_real_and_clean`,
  `test_something_is_actually_decoded`), all "no semantic-trace binary is present and newer than
  every client source file"; `client/` is untouched.
- System Python 3.9.6: `python3 -m unittest discover -s tests` → exit 1, `FAILED (failures=1,
  skipped=11)`: `test_check_fails_on_a_stale_copy_and_update_repairs_it
  (test_readme_status.CommandLineTest)`, the same failure round 021 showed also occurs at that
  round's start commit. Not caused here; not investigated. CI uses 3.10 and 3.12.
- `python3.13 tools/generate_messages.py --check` → exit 0, `message table up to date (96 ids)`.
- `python3.13 tools/generate_protocol_constants.py --check` → exit 0, `protocol constants up to
  date (187 values)`.
- `python3.13 tools/generate_readme_status.py --check` → exit 0, `README status block is up to
  date` (also exit 0 under 3.9.6). The README block did not need regenerating: it is derived from
  the M3 checkbox line, which stays unchecked and unchanged.
- `python3 tools/fw.py check` → exit 0, `0 error(s), 0 warning(s)`.

### Capture (required evidence 8)

`QT_QPA_PLATFORM=offscreen ./ui/build/edopro_next_shell --card-db <synthetic.cdb> --lflist
<synthetic.lflist> --start-screen decks --capture <png>` → exit 0, with a 14-card synthetic
database and a three-entry synthetic banlist, both built in a scratch folder outside the
repository and not committed. I looked at the PNG (1280x800). **Checked visually:** the filter
grid shows below the search box, with Type/Sub-type, Attribute/Type-race, ATK/DEF,
Level-Rank/Scale, Limit, the "Show non-official cards" check box, Effects… and Link markers…
buttons, "Hide filters" and "Clear"; controls that do not apply (sub-type, attribute, race and
the number boxes, card type "Any") are drawn disabled; placeholder texts are readable; the
result list below still has room for four results; nothing overlaps or runs off the pane; the
search heading reads "Search (14 cards loaded)" (the catalog's count). Which cards the list
holds was checked by the tests, not by eye: the capture shows only the first four rows. The first capture showed the check box label almost invisible on the dark
theme; I fixed it and captured again. **Not checked visually:** the two popups open, any
control in a non-default state, the "filters active" label, the window at its 960x600 minimum,
and Qt 6.8.3's rendering.

### Records (required evidence 7): every sentence removed from an architecture document, ADR, the roadmap or capabilities.md

- `card-search.md` §1.2: "This module reproduces only the first half. §2 states exactly which
  pieces of `CheckCardProperties` have a `SearchQuery` equivalent and which are deliberately
  absent." → "`data/` reproduces only the first half. Since round 022 the second half, which
  depends on the selected banlist, is reproduced in `policy/` by `policy::deck_search_admits`
  (§2.3, ADR 0012), and the deck builder applies both. §2 states … deliberately absent from
  `data/`."
- `card-search.md` §2, three rows: token/hidden row's "**None.** Not search - visibility policy.
  `CardSearchIndex` never excludes … upstream's own exclusion of them is unconditional too (§1.2),
  not something "anime mode" or a whitelist ever reveals. … a caller/higher layer that wants
  Token-exclusion has to filter the returned `CardCode`s itself …, which is exactly the kind of
  visibility/policy decision this module deliberately leaves to its caller rather than baking
  in." → "**None in `data/`.** … Reproduced for the deck builder by `policy::deck_search_admits`
  (§2.3), which filters `search()`'s results." (the sentence "upstream's own exclusion of them is
  unconditional too … not something "anime mode" or a whitelist ever reveals" is dropped: it is
  true of tokens and hidden cards but read as if about the whole gate); LFList row's "**None.**
  Legality, explicitly out of scope (§0, CLAUDE.md)." → "**None in `data/`** (legality, §0).
  Reproduced by `policy::deck_search_admits` (§2.3)."; named-categories row's "**None**, as named
  categories - `scope` exposes the same underlying bits as a raw filter, with no "this means
  legal" interpretation attached." → "**None in `data/`** … Reproduced, inside the same
  banlist-dependent branch upstream runs them in, by `policy::deck_search_admits` (§2.3)."
- `card-search.md` §2.1: `type` row "All-bits, uniformly | **Deliberate divergence.** Upstream's
  own operator already differs by card category … reproducing three different
  category-conditional operators for one field was judged not worth the complexity for this
  slice." → "All-bits (`type`), and exact value (`type_equals`, round 022) | **Matches** …";
  `attack`/`defense` row "Six-way per-field scheme … | Three general comparisons … | **Deliberate
  simplification**, already reviewed - the "?" sentinel needs no dedicated case (§9). …" → six
  filter types listed from the source | "Three comparisons plus `excludes_negative`, built from
  the text by `parse_numeric_filter`" | "**Matches every input form** (§2.2) … Before round 022
  this row said …"; `level` row "Same deliberate simplification as attack/defense." →
  "**Matches every input form** (§2.2)."; scale row "Comparison count is the same deliberate
  simplification; the Pendulum gate matches exactly." → "**Matches every input form** (§2.2).
  Upstream's box filters `lscale` only …"; `scope` row: "legality categories" → "categories", and
  a sentence added that the deck builder does not use it; closing paragraph "everything else was
  either already correct or is a previously-reviewed, documented simplification kept as-is." →
  "Round 022 checked the two rows this audit had kept as deliberate … both now match (ADR 0012,
  Decisions 1 and 2)."
- `card-search.md` §4: "Three comparisons - `EqualTo`/`AtLeast`/`AtMost` - not upstream's six-way
  per-field filter-type scheme (…). See §9 for why the "?" sentinel needs no special case here."
  → "Three comparisons … and a flag that makes a negative stored value never match. Together they
  express every one of upstream's six filter types (§2.2) …"; a `type_equals` bullet added.
- `card-search.md` §9: "`NumericFilter` has three comparisons … deliberately smaller than
  upstream's six-way per-field filter-type scheme (§4). The "?" ATK/DEF sentinel … needs no
  special case: … exactly reproducing upstream's own observable result for that case without this
  module knowing what "?" means; a caller specifically wanting only "?" cards uses `EqualTo` with
  `-1` or `-2` directly." → "`NumericFilter` has three comparisons … and `excludes_negative`. …
  Upstream's "at most" forms reject them explicitly (§2.2), which `AtMost` alone does not; that is
  what `excludes_negative` is for. Upstream's "?" input matches `-2` only, not `-1` …"
- `deck-builder-ui.md` §0: "but there is still no artwork, no archetype-name search, no
  structured filters, no controller/gamepad navigation, and no full keyboard parity with
  upstream." → "and search has upstream's filter window (§15 and ADR 0012), but there is still no
  artwork, no archetype-name search or legacy search grammar, no controller/gamepad navigation,
  and no full keyboard parity with upstream."
- `deck-builder-ui.md` §6: "`SearchResultsModel::refresh()` builds a `SearchQuery{ .text =
  queryText_, .limit = 200 }` and calls …" → "builds a `SearchQuery` from `queryText_` and, since
  round 022, the filter choices (§15), and calls …"; "Only one filter exists in this slice: free
  text. No structured filter fell out naturally enough to include without expanding scope, so
  none was added (`SearchQuery`'s other typed fields - `exact_code`, static metadata filters - are
  simply left unset)." → "Round 022 added upstream's filter window (§15) … `exact_code` is still
  left unset."
- `deck-builder-ui.md` §12: "The legacy sigil search grammar / archetype-name resolution
  (`card-search.md`§1.1), and structured filters - only plain text search is wired up." → "The
  legacy sigil search grammar / archetype-name resolution (…), including upstream's card-code
  lookup from the search box. Upstream's filter window is wired up since round 022 (§15), with
  non-descriptive labels for the 32 effect categories …". New §15.
- `deck-placement.md` §6: "Upstream never offers one (§3.2); here the search does show tokens
  (card search is out of this round's scope), so both add buttons are disabled for one and
  `addCardToDeck()` refuses it." → "Upstream never offers one (§3.2). Round 020 left tokens in
  this project's search results, so … Since round 022 the search hides tokens as upstream's does
  (ADR 0012, Decision 4); the refusal stays, for a token reached another way."
- ADR 0011, Decision 2: "this project's search does list tokens (search is out of this round's
  scope)" → "this project's search did list tokens when this ADR was written (search was out of
  that round's scope)", plus a "Superseded in part by ADR 0012, Decision 4" note.
- ROADMAP M3 deck-builder item: "wires `CardCatalog`/`CardSearchIndex` text search, a …" → "…
  text search with upstream's filter window (…, ADR 0012), a …"; "Still missing: artwork, the
  legacy sigil search grammar and structured filters beyond plain text, and full
  keyboard/controller parity." → "Still missing: artwork, the legacy sigil search grammar (with
  archetype-name search and card-code lookup), descriptive labels for the 32 effect-category
  filters, and full keyboard/controller parity."; ADR 0012 added to its links. The checkbox stays
  unchecked.
- `capabilities.md` deck-builder row: "search;" → "search, with upstream's filter window (…);";
  "no structured search filters" → "no legacy search grammar or archetype search, effect
  categories labelled by number"; ADR 0012 link added.
- New: `docs/adr/0012-deck-builder-search-filters.md`.

**Changed test expectations:** none. The only removed test lines are the synthetic `.cdb`
writer in `ui/tests/test_deckbuilder.cpp`, which wrote `race`, `attribute`, `category` as
literal `0,0,0` and now writes new `SyntheticCard` fields that default to 0; every existing
fixture writes the same values as before.

### CI (required evidence 9)

- `gh run view 36334551460` (workflow `edopro-next`, triggered by pushing the branch, head
  `da45ceeefc62ba731a1f0468b526f27d3980c08a`): **conclusion success**. Card and deck data:
  success; Semantic client model: success; Qt 6 shell (Linux, Qt 6.8.3, runs the offscreen
  clean-QML-load check verbatim): success; Regression harness (3.10): success; Regression
  harness (3.12): success; Upstream EDOPro baseline: skipped (runs on master, pull requests,
  weekly and on demand; this round touches neither `gframe/` nor `integration/`).
- CI at the report commit `fw.py report` adds (documentation only) is not known when this file
  is written; my final message says what it was.

## Not verified

- **Upstream was read, not run.** Nothing here ran upstream's deck builder. Every "same as
  upstream" claim rests on transcriptions of the quoted lines in the tests, written by me from
  the same reading as the implementation; a misreading of upstream would appear in both.
- **Irrlicht's combo box after upstream clears and refills the limit list** (which entry is then
  selected): not read. ADR 0012, Decision 6 records this project's behaviour, not a comparison.
- **Labels.** Upstream's labels come from `strings.conf`, which is not in this repository; I did
  not compare wording. Race names are ocgcore's constant names; effect categories are shown as
  bit numbers.
- **The deck builder's search path after `CardSearchIndex`** (per-result database lookup and
  policy check) was not benchmarked.
- **Qt 6.8.3, Linux, Windows**: only CI ran them; this machine has Qt 6.11.1.
- **Visual**: see the capture section's "Not checked visually".
- **Keyboard**: the test checks every filter control accepts Tab focus; it does not drive a real
  Tab sequence, and the popups' keyboard use was not tried.
- Golden reproduction, `client/` cycle, `gframe/` baseline: not run (untouched).
- `python tools/check_pr_evidence.py`: no PR body exists for this branch yet.

## Changed

- `data/include/edopro_next/data/{search_query.h,numeric_filter_text.h}`,
  `data/src/{card_search_index.cpp,numeric_filter_text.cpp}`, `data/CMakeLists.txt`,
  `data/tests/{test_numeric_filter_text.cpp,test_card_search.cpp}`: the parser, the flag,
  `type_equals`, their tests.
- `policy/include/edopro_next/policy/{deck_search_filter.h,lf_list.h}`,
  `policy/src/{deck_search_filter.cpp,lf_list.cpp,deck_validation.cpp}`, `policy/CMakeLists.txt`,
  `policy/tests/test_deck_search_filter.cpp`: the banlist-dependent half; `limitation_for` moved
  (same code; `validate_deck()` results unchanged, its tests untouched and passing).
- `ui/src/deckbuilder/{search_filters.h,search_filters.cpp,search_results_model.h,
  search_results_model.cpp}`, `ui/CMakeLists.txt`, `ui/qml/screens/DeckBuilderScreen.qml`,
  `ui/tests/{test_deckbuilder.cpp,test_deckbuilder_screen.cpp}`: the adapter, the screen, tests.
- Docs as listed above. `docs/state.md` not touched.
- No personal path or email address added (searched the added lines for home-folder, temp-folder
  and address patterns → no match).

Commits after the seat-start commit `1065c21a`: `d93a0715` (data), `c5b91cda` (policy),
`f2e87c11` (ui), `4341b2b4` (policy test message), `f9fd6d07` and `da45ceee` (docs), then this
report.

## Open questions

1. **Effect-category labels.** They read "Category N (0x…)". Descriptive names need upstream's
   string resource or a decision to write this project's own names; either is its own round.
   Listed in the roadmap as still missing.
2. **Numeric search-box code lookup** (a term that is a card code finds that card, bypassing
   every filter, `deck_con.cpp:1096-1106`) is text grammar and was left to the grammar round.
3. **The "show non-official cards" switch is not saved** between runs; upstream saves it. There is
   no settings store in the deck builder yet.
4. **`docs/state.md`** (Brain-owned) still says structured filters are open (its line 23). Not
   touched.
5. **`SearchQuery::scope`** is now unused by the deck builder (the named categories live in
   `policy/`). It is still a tested `data/` capability; I left it.

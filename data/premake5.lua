-- The deck model and .ydk codec (edopro_next_deck) as a separate logical
-- Premake target, mirroring client/premake5.lua's own comment: included
-- only for the observer-enabled legacy build - specifically, the real
-- upstream .ydk interoperability harness in integration/legacy/ needs the
-- real edopro_next::data::save_ydk()/serialize_ydk() this target builds, to
-- prove the bytes it hands to DeckManager::LoadDeckFromFile() are the exact
-- bytes this project's own writer produces, not a hand-authored stand-in.
-- The standalone developer/test build is still owned entirely by
-- data/CMakeLists.txt's own edopro_next_deck target - this file does not
-- replace it, and the two are never built against each other.
--
-- Deliberately only src/ydk.cpp - mirroring data/CMakeLists.txt's own
-- edopro_next_deck/edopro_next_data split (docs/adr/0004-deck-model-ydk-codec.md):
-- this target links no SQLite, and never builds card_database.cpp,
-- text_normalize.cpp or card_search_index.cpp, so it can never gain a
-- card-database, search, Qt, gframe, or ocgcore dependency by accident -
-- the same guarantee the CMake target makes, proven here by what this
-- target actually compiles, not merely by a comment. See
-- docs/adr/0008-upstream-ydk-interop-harness.md for why this second,
-- logical Premake target exists alongside the CMake one rather than either
-- compiling data/src/ydk.cpp a second time directly into integration/legacy/,
-- or trying to make integration/legacy/'s Premake build consume
-- data/CMakeLists.txt's CMake target.
project "edopro_next_deck"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	files { "src/ydk.cpp", "include/**.h" }
	includedirs { "include" }
	warnings "Extra"
	filter { "action:not vs*", "files:**.cpp" }
		enablewarnings "pedantic"
	filter {}

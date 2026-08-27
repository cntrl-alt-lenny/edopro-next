project "edopro_next_legacy_observer"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	files { "*.cpp", "*.h" }
	includedirs {
		"../../client/include",
		"../../gframe",
		"../../ocgcore",
		"."
	}
	links { "edopro_next_client" }
	warnings "Extra"
	filter { "action:not vs*", "files:**.cpp" }
		enablewarnings "pedantic"
	filter {}

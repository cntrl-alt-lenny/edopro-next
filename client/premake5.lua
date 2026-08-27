-- The semantic model is a separate logical target. It is included only for
-- the observer-enabled legacy build; the standalone developer/test build is
-- still owned by client/CMakeLists.txt.
project "edopro_next_client"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	files { "src/**.cpp", "include/**.h" }
	includedirs { "include" }
	warnings "Extra"
	filter { "action:not vs*", "files:**.cpp" }
		enablewarnings "pedantic"
	filter {}

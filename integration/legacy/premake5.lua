project "edopro_next_legacy_observer"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	files { "*.cpp", "*.h" }
	includedirs {
		"../../client/include",
		"../../data/include",
		"../../gframe",
		"../../ocgcore",
		"."
	}
	-- The observer projection includes the real Irrlicht-backed legacy card
	-- headers, and ydk_interop.cpp includes gframe's data_manager.h (for
	-- sqlite3 forward declarations) and talks to sqlite3.h directly to build
	-- its own synthetic database fixture. The workspace-level include
	-- filters apply to gframe's project scope, so mirror only the dependency
	-- include paths needed here - both the irrlicht-specific one this
	-- project already needed, and the base vcpkg include directory
	-- (sqlite3.h lives directly under it, not nested like irrlicht's).
	filter "system:windows"
		externalincludedirs { "../../irrlicht/include" }
	if _OPTIONS["vcpkg-root"] then
		for _, arch in ipairs(archs) do
			filter { "system:not windows", "platforms:" .. arch }
			externalincludedirs { get_vcpkg_root_path(arch) .. "/include/irrlicht", get_vcpkg_root_path(arch) .. "/include" }
		end
	end
	filter { "system:not windows", "options:not vcpkg-root" }
		externalincludedirs { "/usr/include/irrlicht" }
	filter {}
	links { "edopro_next_client", "edopro_next_deck" }
	warnings "Extra"
	filter { "action:not vs*", "files:**.cpp" }
		enablewarnings "pedantic"
	filter {}

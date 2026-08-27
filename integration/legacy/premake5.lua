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
	-- The observer projection includes the real Irrlicht-backed legacy card
	-- headers. The workspace-level include filters apply to gframe's project
	-- scope, so mirror only the dependency include path needed here.
	filter "system:windows"
		externalincludedirs { "../../irrlicht/include" }
	if _OPTIONS["vcpkg-root"] then
		for _, arch in ipairs(archs) do
			filter { "system:not windows", "platforms:" .. arch }
			externalincludedirs { get_vcpkg_root_path(arch) .. "/include/irrlicht" }
		end
	end
	filter { "system:not windows", "options:not vcpkg-root" }
		externalincludedirs { "/usr/include/irrlicht" }
	filter {}
	links { "edopro_next_client" }
	warnings "Extra"
	filter { "action:not vs*", "files:**.cpp" }
		enablewarnings "pedantic"
	filter {}

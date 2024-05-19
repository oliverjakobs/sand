workspace "Sand"
    architecture "x64"
    startproject "Sand"

    configurations
    {
        "Debug",
        "OptimizedDebug",
        "Release"
    }

    flags { "MultiProcessorCompile" }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "On"
        
    filter "configurations:OptimizedDebug"
        runtime "Debug"
        symbols "On"
        optimize "On"

    filter "configurations:Release"
        runtime "Release"
        optimize "On"

output_dir = "%{cfg.buildcfg}"

group "Packages"

include "packages/Ignis.lua"

group ""

project "Sand"
    kind "ConsoleApp"
	language "C"
	cdialect "C99"
    staticruntime "On"
    
    targetdir ("build/bin/" .. output_dir .. "/%{prj.name}")
    objdir ("build/bin-int/" .. output_dir .. "/%{prj.name}")

    files
    {
        --Source
        "src/**.h",
        "src/**.c",
        --Resources
        "res/shaders/**.vert",
        "res/shaders/**.frag"
    }

    links
    {
        "Ignis",
        "opengl32"
    }

    includedirs
    {
        "src",
        "packages/Ignis/src",
        "packages/minimal",
    }

    defines
    {
        "MINIMAL_PLATFORM_WINDOWS"
    }

    filter "system:linux"
        links { "dl", "pthread" }
        defines { "_X11" }

    filter "system:windows"
        systemversion "latest"
        defines { "WINDOWS", "_CRT_SECURE_NO_WARNINGS" }

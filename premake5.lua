workspace "prlib"
    location "build"
    configurations { "Debug", "Release" }
    
    startproject "prlib_example"

architecture "x86_64"

project "prlib"
    kind "StaticLib"
    language "C++"
    targetdir "bin/"
    systemversion "latest"
    flags { "MultiProcessorCompile", "NoPCH" }

    -- GLFW
    files {
        "libs/glfw3/src/internal.h", 
        "libs/glfw3/src/mappings.h", 
        "libs/glfw3/include/GLFW/glfw3.h", 
        "libs/glfw3/include/GLFW/glfw3native.h", 
        "libs/glfw3/src/glfw_config.h", 
        "libs/glfw3/src/context.c", 
        "libs/glfw3/src/init.c", 
        "libs/glfw3/src/input.c", 
        "libs/glfw3/src/monitor.c", 
        "libs/glfw3/src/vulkan.c", 
        "libs/glfw3/src/window.c",
    }
    filter { "system:Windows" }
    files {
        "libs/glfw3/src/win32_platform.h",
        "libs/glfw3/src/win32_joystick.h",
        "libs/glfw3/src/win32_joystick.c",
        "libs/glfw3/src/wgl_context.h",
        "libs/glfw3/src/egl_context.h",
        "libs/glfw3/src/egl_context.c",
        "libs/glfw3/src/osmesa_context.h",
        "libs/glfw3/src/osmesa_context.c",
        "libs/glfw3/src/win32_init.c",
        "libs/glfw3/src/win32_monitor.c",
        "libs/glfw3/src/win32_time.h",
        "libs/glfw3/src/win32_time.c",
        "libs/glfw3/src/win32_thread.h",
        "libs/glfw3/src/win32_thread.c",
        "libs/glfw3/src/win32_window.c",
        "libs/glfw3/src/wgl_context.h",
        "libs/glfw3/src/wgl_context.c",
    }
    filter { "system:Linux" }
    files {
        "libs/glfw3/src/x11_platform.h",
        "libs/glfw3/src/linux_joystick.h",
        "libs/glfw3/src/linux_joystick.c",
        "libs/glfw3/src/glx_context.c",
        "libs/glfw3/src/egl_context.c",
        "libs/glfw3/src/osmesa_context.h",
        "libs/glfw3/src/osmesa_context.c",
        "libs/glfw3/src/x11_init.c",
        "libs/glfw3/src/x11_monitor.c",
        "libs/glfw3/src/posix_time.h",
        "libs/glfw3/src/posix_time.c",
        "libs/glfw3/src/posix_thread.h",
        "libs/glfw3/src/posix_thread.c",
        "libs/glfw3/src/x11_window.c",
        "libs/glfw3/src/glx_context.h",
        "libs/glfw3/src/glx_context.c",
        "libs/glfw3/src/xkb_unicode.h",
        "libs/glfw3/src/xkb_unicode.c"
    }
    filter {}

    includedirs { "libs/glfw3/include" }
    includedirs { "libs/glfw3/src/" }

    filter { "system:Windows" }
        defines { "_GLFW_WIN32" }
    filter { "system:Linux" }
        defines { "_GLFW_X11" }
    filter {}

    -- GLM
    -- Disabled. Because source codes should not change behavior w/o GLM_FORCE_CTOR_INIT
    -- defines { "GLM_FORCE_CTOR_INIT", "GLM_FORCE_PURE" }

    -- GLEW
    files {"libs/glew/src/*.c"}
    files {"libs/glew/include/GL/*.h"}
    includedirs { "libs/glew/include" }
    defines { "GLEW_STATIC" }

    -- cwalk
    files {"libs/cwalk/*.c"}
    files {"libs/cwalk/*.h"}
    includedirs { "libs/cwalk" }

    -- ImGui
    includedirs { "libs/imgui" }
    files {"libs/imgui/*.cpp"}

    -- lib
    includedirs { "libs/stb" }
    files {"libs/stb/*.h"}

    -- Imath, OpenEXR, Alembic, zlib
    files { "libs/src_Imath/**.cpp" }
    files { "libs/src_Imath/**.h" }
    files { "libs/src_zlib/**.c" }
    files { "libs/src_zlib/**.h" }
    files { "libs/src_openexr/**.cpp" }
    files { "libs/src_openexr/**.c" }
    files { "libs/src_openexr/**.h" }
    files { "libs/src_alembic/**.cpp" }
    files { "libs/src_alembic/**.h" }
    includedirs { "libs/src_configs", "libs/src_Imath", "libs/src_zlib", "libs/src_openexr/OpenEXR", "libs/src_alembic"}

    -- tinyobjloader
    includedirs { "libs/tiny_obj_loader" }
    files { "libs/tiny_obj_loader/tiny_obj_loader.cc" }
    files { "libs/tiny_obj_loader/tiny_obj_loader.h" }

    -- xxhash
    includedirs { "libs/xxhash" }
    files { "libs/xxhash/xxhash.c" }
    files { "libs/xxhash/xxhash.h" }

    -- xxhash
    includedirs { "libs/libattopng" }
    files { "libs/libattopng/libattopng.c" }
    files { "libs/libattopng/libattopng.h" }

    -- Lib Core
    files {"src/*.cpp", "src/*.h", "src/*.hpp"}
    includedirs { "src/" }
    files {"src/*.natvis"}

    filter { "system:Windows" }
        links { "opengl32" }
    filter {}
    
    filter {"Debug"}
        runtime "Debug"
        targetname ("prlib_d")
        optimize "Off"
        -- optimize "Full"
    filter {"Release"}
        runtime "Release"
        targetname ("prlib")
        optimize "Full"
    filter{}

    -- strcpy_s
    disablewarnings { "4996"} 

    -- int overflow 
    disablewarnings { "26451"} 
project "prlib_example"
    kind "ConsoleApp"
    language "C++"
    targetdir "bin/"
    systemversion "latest"
    flags { "MultiProcessorCompile", "NoPCH" }

    filter { "system:Linux" }
        links { "GL", "pthread", "m", "X11", "dl"}
    filter {}

    -- Src
    files { "example/main.cpp" }

    -- UTF8
    postbuildcommands { 
        "mt.exe -manifest ../utf8.manifest -outputresource:\"$(TargetDir)$(TargetName).exe\" -nologo"
    }

    -- lib
    dependson { "prlib" }
    links { "prlib" }
    includedirs { "src/" }

    -- embree 3
    -- libdirs { "libs/embree3/lib" }
    -- includedirs { "libs/embree3/include" }
    -- links{
    --     "embree3",
    --     "tbb",
    -- }
    -- postbuildcommands {
    --     "{COPYFILE} ../libs/embree3/bin/*.dll %{cfg.targetdir}/*.dll",
    -- }
    
    -- embree 4
    libdirs { "libs/embree4/lib" }
    includedirs { "libs/embree4/include" }
    links{
        "embree4",
        "tbb",
    }
    postbuildcommands {
        "{COPYFILE} ../libs/embree4/bin/*.dll %{cfg.targetdir}/*.dll",
    }


    symbols "On"

    filter {"Debug"}
        runtime "Debug"
        targetname ("prlib_example_Debug")
        optimize "Off"
    filter {"Release"}
        runtime "Release"
        targetname ("prlib_example")
        optimize "Full"
    filter{}

project "sandbox"
    kind "ConsoleApp"
    language "C++"
    targetdir "bin/"
    systemversion "latest"
    flags { "MultiProcessorCompile", "NoPCH" }

    filter { "system:Linux" }
        links { "GL", "pthread", "m", "X11", "dl"}
    filter {}

    -- Src
    files { "sandbox/main.cpp" }

    -- lib
    includedirs { "libs/stb" }

    symbols "On"

    filter {"Debug"}
        runtime "Debug"
        targetname ("sandbox_Debug")
        optimize "Off"
    filter {"Release"}
        runtime "Release"
        targetname ("sandbox")
        optimize "Full"
    filter{}

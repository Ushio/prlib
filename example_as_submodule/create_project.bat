git init
git submodule add https://github.com/Ushio/prlib libs/prlib
copy libs\prlib\premake5.exe premake5.exe
copy libs\prlib\example_as_submodule\premake5.lua premake5.lua
copy libs\prlib\example_as_submodule\main.cpp main.cpp
premake5.exe vs2017
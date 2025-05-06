@echo off

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

set IncludeDir=w:\core\src\core\
set LibDir=
set Libs=Ws2_32.lib
set Warnings= -wd4505 -wd4005 -wd4457
set CompilerFlags=-nologo -Od -WX -W4 -std:c++17 -FAsc -Zi -GR-  %Warnings%
set Macros=-DBUILD_DEBUG

set LinkerFlags=-INCREMENTAL:no

cl %Macros% %CompilerFlags% ..\src\server.cpp -I %IncludeDir% /link %LinkerFlags% %Libs%
cl %Macros% %CompilerFlags% ..\src\client.cpp -I %IncludeDir% /link %LinkerFlags% %Libs%

rem .\test.exe
popd


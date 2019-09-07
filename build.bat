@echo off
                        set "VSPATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community"
if not exist "%VSPATH%" set "VSPATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise"
if not exist "%VSPATH%" set "VSPATH=C:\Program Files (x86)\Microsoft Visual Studio\2017\Community"
if not exist "%VSPATH%" set "VSPATH=C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise"

if %VisualStudioVersion%. == . call "%VSPATH%\Common7\Tools\VsDevCmd.bat" -host_arch=amd64 -arch=amd64

if not exist build mkdir build 
pushd build
cmake -DCMAKE_GENERATOR_PLATFORM=x64 ..
devenv cppreflect.sln /build "Release|x64"
popd

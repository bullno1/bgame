@echo on

for /f "usebackq tokens=*" %%i in (`"%~dp0vswhere.exe" -latest -prerelease -property installationPath`) do (
  set VS_DIR=%%i
)

call "%VS_DIR%\VC\Auxiliary\Build\vcvarsall.bat" x64

mkdir .build\win
if not defined GENERATOR set "GENERATOR=Visual Studio 18 2026"
cmake ^
    -S . ^
    -B .build\win ^
    -G "%GENERATOR%" ^
    -D RELOADABLE=OFF ^
    -DCMAKE_C_COMPILER_LAUNCHER=sccache ^
    -DCMAKE_CXX_COMPILER_LAUNCHER=sccache ^
    -D PLATFORM_NAME=win ^
    -D "CMAKE_TOOLCHAIN_FILE=%~dp0..\..\cmake\msvc.cmake" ^
    -D CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded

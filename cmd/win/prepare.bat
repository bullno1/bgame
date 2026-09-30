@echo on

call "%~dp0env.bat" || exit /b 1

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
    -D PLATFORM_NAME=win ^
    -D "CMAKE_TOOLCHAIN_FILE=%BGAME_DIR%\cmake\msvc.cmake" ^
    -D CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded

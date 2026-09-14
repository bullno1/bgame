call "%~dp0env.bat" || exit /b 1

if not exist ".build\win\CMakeCache.txt" (
    call "%~dp0prepare.bat"
)

cmake --build .build\win --config %BUILD_TYPE% --parallel

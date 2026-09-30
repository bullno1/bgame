@rem Reads bgame.env from the current directory (the project root) into
@rem environment variables. See cmd/common.sh for the keys.
@if not exist bgame.env (
    echo bgame.env not found: run from the project root 1>&2
    exit /b 1
)
@for /f "usebackq tokens=1,* delims== eol=#" %%a in ("bgame.env") do @set "%%a=%%~b"
@if not defined BUILD_TYPE set "BUILD_TYPE=RelWithDebInfo"
@rem cmd.exe resolves ".." textually, so %~dp0..\.. is wrong when cmd is a
@rem symlink into the submodule.
@if not defined BGAME_DIR if exist "%~dp0..\..\cmake\msvc.cmake" set "BGAME_DIR=%~dp0..\.."
@if not defined BGAME_DIR set "BGAME_DIR=%CD%\deps\bgame"

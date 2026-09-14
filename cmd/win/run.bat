call "%~dp0env.bat" || exit /b 1

bin\win\%BUILD_TYPE%-static\%APP%.exe %*

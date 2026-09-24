@echo off
REM LSP Color Palette — Windows build (Ninja + MSVC).
REM Output: release\LSP_Color_Palette_<version>_windows\LSP_Color_Palette_<version>.ofx.bundle
REM Usage: tools\windows\colorpalette_build.bat
REM        tools\windows\colorpalette_build.bat clean

setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
  call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
  if errorlevel 1 exit /b 1
)

cd /d "%~dp0..\..\"

cmake -S . -B build/windows -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
if errorlevel 1 exit /b 1

if /i "%~1"=="clean" (
  cmake --build build/windows --target clean
  if errorlevel 1 exit /b 1
)

cmake --build build/windows --target colorpalette_all --parallel
exit /b %ERRORLEVEL%

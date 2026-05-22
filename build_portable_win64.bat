@echo off
setlocal EnableExtensions

REM Portable Win64 build - does NOT replace your installed OrcaSlicer program.
REM Output: dist\MyOrcaSlicer-portable\
REM Uses local data_dir for isolated profiles (separate from %%APPDATA%%\OrcaSlicer).
REM After build, copy profiles once:  scripts\copy_profiles_to_portable.bat

set WP=%CD%
set BUILD_DIR=build-portable
set BUILD_TYPE=Release
set INSTALL_DIR=%WP%\dist\MyOrcaSlicer-portable
set DEPS_DIR=%WP%\deps\build
set DEPS_PREFIX=%DEPS_DIR%\OrcaSlicer_dep\usr\local

set "PATH=C:\Strawberry\perl\bin;C:\Program Files\CMake\bin;%PATH%"

if not exist "%DEPS_PREFIX%\lib\cmake\Boost-1.84.0" (
    echo Dependencies not built. Run build_release.bat first.
    exit /b 1
)

if not exist "%DEPS_PREFIX%\lib\libcurl.lib" (
    echo Building dep_CURL...
    pushd "%DEPS_DIR%"
    cmake --build . --config %BUILD_TYPE% --target dep_CURL -- -m
    if errorlevel 1 exit /b 1
    popd
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

if not exist CMakeCache.txt (
    cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_INSTALL_PREFIX="%INSTALL_DIR%" -DDEP_BUILD_DIR="%DEPS_DIR%" -DCMAKE_PREFIX_PATH="%DEPS_PREFIX%"
    if errorlevel 1 exit /b 1
) else (
    cmake .. -DDEP_BUILD_DIR="%DEPS_DIR%" -DCMAKE_PREFIX_PATH="%DEPS_PREFIX%" -DCMAKE_INSTALL_PREFIX="%INSTALL_DIR%"
    if errorlevel 1 exit /b 1
)

cmake --build . --config %BUILD_TYPE% --target ALL_BUILD -- -m
if errorlevel 1 exit /b 1

cmake --build . --target install --config %BUILD_TYPE%
if errorlevel 1 exit /b 1

cd "%WP%"

if not exist "%INSTALL_DIR%\data_dir" mkdir "%INSTALL_DIR%\data_dir"
echo Portable data_dir: %INSTALL_DIR%\data_dir

echo.
echo Portable build ready: %INSTALL_DIR%
echo Run: %INSTALL_DIR%\orca-slicer.exe
echo To refresh profiles from installed OrcaSlicer: scripts\copy_profiles_to_portable.bat
echo.

endlocal

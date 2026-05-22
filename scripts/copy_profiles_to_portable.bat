@echo off
setlocal EnableExtensions

REM Copy printer/preset profiles from installed OrcaSlicer to portable data_dir.
REM Does not touch %%APPDATA%%\OrcaSlicer (your installed app keeps using that).

set "SRC=%APPDATA%\OrcaSlicer"
set "DST=%~dp0..\dist\MyOrcaSlicer-portable\data_dir"

if not exist "%SRC%" (
    echo Source not found: %SRC%
    exit /b 1
)

if not exist "%DST%" mkdir "%DST%"

echo Copying profiles from:
echo   %SRC%
echo to:
echo   %DST%
echo.
echo Close OrcaSlicer portable before copying if files are locked.
echo.

for %%D in (system user printers) do (
    if exist "%SRC%\%%D" (
        echo Copying %%D ...
        robocopy "%SRC%\%%D" "%DST%\%%D" /E /IS /IT /NFL /NDL /NJH /NJS /nc /ns /np
    )
)

if exist "%SRC%\OrcaSlicer.conf" (
    echo Copying OrcaSlicer.conf ...
    copy /Y "%SRC%\OrcaSlicer.conf" "%DST%\OrcaSlicer.conf"
)

echo.
echo Done. Start portable OrcaSlicer from dist\MyOrcaSlicer-portable\orca-slicer.exe
echo.

endlocal

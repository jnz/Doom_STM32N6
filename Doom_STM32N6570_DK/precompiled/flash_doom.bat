@echo off
setlocal

:: --- File paths (pre-signed binaries in current folder, WAD one level up) ---
set FSBL_TRUSTED=Doom_FSBL-Trusted.bin
set APPS_TRUSTED=Doom_AppS-Trusted.bin
set WAD_FILE=..\wad\DOOM1.WAD
set WAD_BIN=..\wad\DOOM1.bin

:: --- Flash target addresses ---
set FSBL_ADDR=0x70000000
set APPS_ADDR=0x70100000
set WAD_ADDR=0x70300000

:: --- Programmer / External Loader setup ---
set EL=%STM32CLT_PATH%/STM32CubeProgrammer/bin/ExternalLoader/MX66UW1G45G_STM32N6570-DK.stldr
set CONNECT=-c port=SWD mode=HOTPLUG -el "%EL%"

:: =========================================================================
::  Pre-flight checks
:: =========================================================================
if not exist "%FSBL_TRUSTED%" (
    echo ERROR: %FSBL_TRUSTED% not found in current directory!
    exit /b 1
)
if not exist "%APPS_TRUSTED%" (
    echo ERROR: %APPS_TRUSTED% not found in current directory!
    exit /b 1
)
if not exist "%WAD_FILE%" (
    echo ERROR: %WAD_FILE% not found!
    exit /b 1
)

echo.
echo =========================================================
echo  DOOM for STM32N6570-DK  -  Full Flash Script
echo =========================================================
echo.
echo  Files:
echo    FSBL : %FSBL_TRUSTED%  -^> %FSBL_ADDR%
echo    AppS : %APPS_TRUSTED%  -^> %APPS_ADDR%
echo    WAD  : %WAD_FILE%      -^> %WAD_ADDR%
echo.
echo =========================================================
echo  STEP 1: Set board to DEVELOPMENT MODE
echo    BOOT1 = 1-3  (pins 1 and 3 bridged)
echo    BOOT0 = don't care
echo  Then reset or power-cycle the board.
echo =========================================================
echo.
pause

:: =========================================================================
::  Flash FSBL
:: =========================================================================
echo === Flashing FSBL to %FSBL_ADDR% ===
STM32_Programmer_CLI.exe %CONNECT% -d %FSBL_TRUSTED% %FSBL_ADDR%
if errorlevel 1 goto error

:: =========================================================================
::  Flash AppS
:: =========================================================================
echo === Flashing AppS to %APPS_ADDR% ===
STM32_Programmer_CLI.exe %CONNECT% -d %APPS_TRUSTED% %APPS_ADDR%
if errorlevel 1 goto error

:: =========================================================================
::  Flash DOOM1.WAD (copy to .bin first, clean up after)
:: =========================================================================
echo === Copying WAD to temporary .bin ===
copy /Y "%WAD_FILE%" "%WAD_BIN%"
if errorlevel 1 goto error

echo === Flashing DOOM1.WAD to %WAD_ADDR% ===
STM32_Programmer_CLI.exe %CONNECT% -d %WAD_BIN% %WAD_ADDR%
if errorlevel 1 goto error_cleanup

echo === Cleaning up temporary file ===
del "%WAD_BIN%"

:: =========================================================================
::  Done
:: =========================================================================
echo.
echo =========================================================
echo  All three images flashed successfully!
echo.
echo  STEP 2: Set board to BOOT FROM FLASH
echo    BOOT0 = 1-2  (pins 1 and 2 bridged)
echo    BOOT1 = 1-2  (pins 1 and 2 bridged)
echo  Then reset or power-cycle the board.
echo =========================================================
echo.
goto end

:error_cleanup
del "%WAD_BIN%" 2>nul

:error
echo.
echo === ERROR: Flashing aborted! ===
exit /b 1

:end
endlocal

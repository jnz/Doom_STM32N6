@echo off
setlocal

set FSBL_BIN=STM32CubeIDE\Boot\Debug\Template_XIP_FSBL.bin
set APPS_BIN=STM32CubeIDE\AppS\Debug\Template_XIP_AppS.bin
set FSBL_TRUSTED=STM32CubeIDE\Boot\Debug\Template_XIP_FSBL-Trusted.bin
set APPS_TRUSTED=STM32CubeIDE\AppS\Debug\Template_XIP_AppS-Trusted.bin

set EL=%STM32CLT_PATH%/STM32CubeProgrammer/bin/ExternalLoader/MX66UW1G45G_STM32N6570-DK.stldr
:: set CONNECT=-c port=SWD mode=HOTPLUG ap=1 -el "%EL%"
set CONNECT=-c port=SWD mode=HOTPLUG -el "%EL%"

echo.
echo =======================================================
echo  SCHRITT 1: Board auf DEVELOPMENT MODE setzen
echo  BOOT1 = 1-3 (Pins 1 und 3 gebrueckt)
echo  BOOT0 = egal
echo  Dann Board per Reset-Knopf oder Power-Cycle neu starten
echo =======================================================
echo

echo === Signing FSBL ===
STM32_SigningTool_CLI.exe -s -bin %FSBL_BIN% -nk -of 0x80000000 -t fsbl -o %FSBL_TRUSTED% -hv 2.3 -dump %FSBL_TRUSTED% -align
if errorlevel 1 goto error

echo === Signing AppS ===
STM32_SigningTool_CLI.exe -s -bin %APPS_BIN% -nk -of 0x80000000 -t fsbl -o %APPS_TRUSTED% -hv 2.3 -dump %APPS_TRUSTED% -align
if errorlevel 1 goto error

echo === Flashing FSBL to 0x70000000 ===
STM32_Programmer_CLI.exe %CONNECT% -d %FSBL_TRUSTED% 0x70000000
if errorlevel 1 goto error

echo === Flashing AppS to 0x70100000 ===
STM32_Programmer_CLI.exe %CONNECT% -d %APPS_TRUSTED% 0x70100000
if errorlevel 1 goto error

echo.
echo =======================================================
echo  SCHRITT 2: Board auf BOOT FROM FLASH setzen
echo  BOOT0 = 1-2 (Pins 1 und 2 gebrueckt)
echo  BOOT1 = 1-2 (Pins 1 und 2 gebrueckt)
echo  Dann Board per Reset-Knopf oder Power-Cycle neu starten
echo =======================================================
echo.
echo === Done! Both binaries signed and flashed successfully ===
goto end

:error
echo.
echo === ERROR! Process aborted ===
exit /b 1

:end
endlocal


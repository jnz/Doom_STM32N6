@echo off
setlocal

set WAD_FILE=wad\DOOM1.WAD
set BIN_FILE=wad\DOOM1.bin
set TARGET_ADDR=0x70300000

set EL=%STM32CLT_PATH%/STM32CubeProgrammer/bin/ExternalLoader/MX66UW1G45G_STM32N6570-DK.stldr

set CONNECT=-c port=SWD mode=HOTPLUG -el "%EL%"

if not exist "%WAD_FILE%" (
    echo ERROR: %WAD_FILE% nicht gefunden!
    echo Bitte DOOM1.WAD in das gleiche Verzeichnis wie dieses Script legen.
    exit /b 1
)

echo.
echo =======================================================
echo  DOOM1.WAD auf STM32N6570-DK XSPI Flash schreiben
echo =======================================================
echo.
echo  Zieladresse: %TARGET_ADDR%
echo  Datei:       %WAD_FILE%
echo.
echo  BOOT1 = 1-3 (Pins 1 und 3 gebrueckt)
echo  Board per Reset-Knopf oder Power-Cycle neu starten
echo =======================================================
echo.

echo === Kopiere %WAD_FILE% nach %BIN_FILE% ===
copy /Y "%WAD_FILE%" "%BIN_FILE%"
if errorlevel 1 goto error

echo === Flashing %BIN_FILE% to %TARGET_ADDR% ===
STM32_Programmer_CLI.exe %CONNECT% -d %BIN_FILE% %TARGET_ADDR%
if errorlevel 1 goto error

echo === Raeume temporaere Datei auf ===
del "%BIN_FILE%"

echo.
echo =======================================================
echo  DOOM1.WAD erfolgreich geflasht!
echo.
echo  Board wieder auf BOOT FROM FLASH setzen:
echo  BOOT0 = 1-2 (Pins 1 und 2 gebrueckt)
echo  BOOT1 = 1-2 (Pins 1 und 2 gebrueckt)
echo  Dann Board per Reset-Knopf oder Power-Cycle neu starten
echo =======================================================
echo.
goto end

:error
echo.
echo === ERROR! Flash-Vorgang abgebrochen ===
exit /b 1

:end
endlocal

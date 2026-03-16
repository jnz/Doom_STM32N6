@echo off
setlocal

set FSBL_BIN=STM32CubeIDE\Boot\Debug\Template_XIP_FSBL.bin
set APPS_BIN=STM32CubeIDE\AppS\Debug\Template_XIP_AppS.bin
set FSBL_TRUSTED=STM32CubeIDE\Boot\Debug\Template_XIP_FSBL-Trusted.bin
set APPS_TRUSTED=STM32CubeIDE\AppS\Debug\Template_XIP_AppS-Trusted.bin

set EL=%STM32CLT_PATH%/STM32CubeProgrammer/bin/ExternalLoader/MX66UW1G45G_STM32N6570-DK.stldr
:: set CONNECT=-c port=SWD mode=HOTPLUG ap=1 -el "%EL%"
set CONNECT=-c port=SWD mode=HOTPLUG -el "%EL%"

echo === Reading FSBL from 0x70000000 ===
STM32_Programmer_CLI.exe %CONNECT% -r 0x70000000 0x8000 dump_fsbl.bin
if errorlevel 1 goto error

goto end

:error
echo.
echo === ERROR! Process aborted ===
exit /b 1

:end
endlocal


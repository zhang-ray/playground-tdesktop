
setlocal EnableDelayedExpansion
set "FullScriptPath=%~dp0"
set "FullExecPath=%cd%"
set "BuildTarget="



set GYP_MSVS_VERSION=2017

cd "%FullScriptPath%"
call gyp --depth=. --generator-output=.. -Goutput_dir=../out !BUILD_DEFINES! -Dofficial_build_target=%BuildTarget% Telegram.gyp --format=ninja
if %errorlevel% neq 0 goto error
call gyp --depth=. --generator-output=.. -Goutput_dir=../out !BUILD_DEFINES! -Dofficial_build_target=%BuildTarget% Telegram.gyp --format=msvs-ninja
if %errorlevel% neq 0 goto error
cd ../..

rem looks like ninja build works without sdk 7.1 which was used by generating custom environment.arch files

cd "%FullExecPath%"
exit /b

:error
echo FAILED
cd "%FullExecPath%"
exit /b 1

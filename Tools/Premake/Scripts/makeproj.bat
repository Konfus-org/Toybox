@echo off

:: Running premake to generate new solution and proj files
set "scriptDir=%~dp0..\..\.."
echo:
echo Starting premake..
echo:
cd "%scriptDir%"
.\Tools\Premake\premake5.exe vs2022
if not ["%errorlevel%"]==["0"] (
    echo:
    echo Project build failed...
    echo:
    pause
)
echo:
echo Success!
echo:
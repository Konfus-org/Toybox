@echo off
echo:
echo Starting premake..
echo:
"3rd Party"\Tools\premake\premake5.exe vs2022
if not ["%errorlevel%"]==["0"] (
    echo:
    echo Project build failed...
    echo:
    pause
)
echo:
echo Success!
echo:
timeout /t 3
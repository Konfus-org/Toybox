@echo off

:: Deleting old vs proj files
set "scriptDir=%~dp0"

echo Deleting Visual Studio project and solution files in %scriptDir% and its subdirectories...

for /r "%scriptDir%" %%f in (*.vcxproj *.csproj *.vcproj *.sln) do (
    echo Deleting %%f
    del "%%f"
)

echo Done.
endlocal

:: Running premake to generate new solution and proj files
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
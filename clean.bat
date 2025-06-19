@echo off

:: Deleting old vs proj files
set "scriptDir=%~dp0"
echo Deleting Visual Studio project and solution files in %scriptDir% and its subdirectories...

for /r "%scriptDir%" %%f in (*.vcxproj *.csproj *.vcproj *.sln *.vcxproj.filters *.vcxproj.user) do (
    echo Deleting %%f
    del "%%f"
)

echo Done.
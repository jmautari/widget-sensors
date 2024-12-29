@echo off
setlocal

rem Set project name below
set PROJECT_NAME=widget-sensors

if not exist "build\%PROJECT_NAME%.sln" goto need_to_create_build_solution

PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%

for /f "usebackq delims=" %%i in (`vswhere.exe -latest -version "[17.0,17.99]" -requires Microsoft.Component.MSBuild -property installationPath`) do (
  set InstallDir=%%i
  if not exist "%%i\MSBuild\Current\Bin\MSBuild.exe" (
    echo error: Cannot locate Visual Studio 2022.
    goto error_out
  )
)

:compile
set __VCVARS=%InstallDir%\VC\Auxiliary\Build

call "%__VCVARS%\vcvarsall.bat" amd64

cd build
msbuild %PROJECT_NAME%.sln /p:Configuration=Release %*
cd ..

goto done

:need_to_create_build_solution
echo Run create_solution.bat at least once before building the project
goto done

:error_out
echo Error locating MSBuild
goto done

:done

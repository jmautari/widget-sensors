@echo off
setlocal

if not exist "build\getthumb.sln" goto need_to_create_build_solution

PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%

for /f "usebackq delims=" %%i in (`vswhere.exe -latest -version "[16.4,16.99]" -requires Microsoft.Component.MSBuild -property installationPath`) do (
  set InstallDir=%%i
  if not exist "%%i\MSBuild\Current\Bin\MSBuild.exe" (
    echo error: Cannot locate Visual Studio 2019.
    goto error_out
  )
)

:compile
set __VCVARS=%InstallDir%\VC\Auxiliary\Build

call "%__VCVARS%\vcvarsall.bat" amd64

cd build
msbuild getthumb.sln /p:Configuration=Release %*
cd ..

goto done

:need_to_create_build_solution
echo Run create_solution.bat at least once before building the project
goto done

:error_out
echo Error locating MSBuild
goto done

:done

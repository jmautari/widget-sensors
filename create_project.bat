@echo off
setlocal

PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%

for /f "usebackq delims=" %%i in (`vswhere.exe -latest -version "[16.4,16.79]" -requires Microsoft.Component.MSBuild -property installationPath`) do (
  set InstallDir=%%i
  if not exist "%%i\MSBuild\Current\Bin\MSBuild.exe" (
    echo error: Cannot locate Visual Studio 2019.
    goto error_out
  )
)

set __VCVARS=%InstallDir%\VC\Auxiliary\Build

call "%__VCVARS%\vcvarsall.bat" amd64 10.0.19041.0 -vcvars_ver=14.2 %*

:build
mkdir build

cd build
cmake -Wno-dev -DCMAKE_PREFIX_PATH=../third_party/fmt/support/cmake -G "Visual Studio 16" -A x64 -DCMAKE_SYSTEM_VERSION=10.0.19041.0 ..
cd ..
goto done

:error
echo Project creation failure.

:done

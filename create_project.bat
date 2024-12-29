@echo off
setlocal

PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%

for /f "usebackq delims=" %%i in (`vswhere.exe -latest -version "[17.0,17.99]" -requires Microsoft.Component.MSBuild -property installationPath`) do (
  set InstallDir=%%i
  if not exist "%%i\MSBuild\Current\Bin\MSBuild.exe" (
    echo error: Cannot locate Visual Studio 2022.
    goto error_out
  )
)

set __VCVARS=%InstallDir%\VC\Auxiliary\Build
echo %__VCVARS%

call "%__VCVARS%\vcvarsall.bat" amd64 10.0.26100.0

:build
mkdir build

cd build
if exist CMakeCache.txt (
  echo Deleting existing CMakeCache.txt
  del CMakeCache.txt
)

cmake -Wno-dev -DCMAKE_PREFIX_PATH=../third_party/fmt/support/cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_VERSION=10.0.26100.0 ..
cd ..
goto done

:error
echo Project creation failure.

:done

REM Copyright Red Hat 2007-2011
REM Authors: Yan Vugenfirer
REM          Arnon Gilboa
REM          Uri Lublin
:
: Set global parameters: 
:

: Use Windows 7 DDK
if "%DDKVER%"=="" set DDKVER=7600.16385.0

: By default DDK is installed under C:\WINDDK, but it can be installed in different location
if "%DDKINSTALLROOT%"=="" set DDKINSTALLROOT=C:\WINDDK\
set BUILDROOT=%DDKINSTALLROOT%%DDKVER%
set X64ENV=x64
if "%DDKVER%"=="6000" set X64ENV=amd64
if "%BUILDCFG%"=="" set BUILDCFG=fre

if not "%1"=="" goto parameters_here
echo no parameters specified, exiting
goto :eof
:parameters_here

:nextparam
if "%1"=="" goto :eof
goto %1
:continue
shift
goto nextparam

:fre
set BUILDCFG=fre
goto continue

:chk
set BUILDCFG=chk
goto continue

:Win7
set BUILDENV=WIN7
goto build_it

:Win7_64
set BUILDENV=%X64ENV% WIN7
goto build_it

:Vista
set BUILDENV=Wlh
goto build_it

:Vista64
set BUILDENV=%X64ENV% Wlh
goto build_it

:Win2003
set BUILDENV=WNET
goto build_it

:Win200364
set BUILDENV=%X64ENV% WNET
goto build_it

:XP
set BUILDENV=WXP
goto build_it

:build_it
set DDKBUILDENV=
pushd %BUILDROOT%
call %BUILDROOT%\bin\setenv.bat %BUILDROOT% %BUILDCFG% %BUILDENV%
popd
build -cZg

goto continue


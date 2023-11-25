@echo off

Rem
Rem Build and copy to target based on environment variables set by WinDDK\bin\setenv.
Rem if an argument is supplied the build products are copied there too under a subdirectory.
Rem

Rem Just use %BUILD_ALT_DIR%, it is equivalent
Rem set TARGET=%DDK_TARGET_OS%_%_BUILDARCH%_%BUILD_TYPE%

if not DEFINED BUILD_ALT_DIR (
 echo BUILD_ALT_DIR not defined. Please run in a WinDDK Build Environment.
 goto exit
)
if not DEFINED SPICE_COMMON_DIR (
 set SPICE_COMMON_DIR=%CD%\..\spice-protocol
)

set TARGET=install_%BUILD_ALT_DIR%
echo TARGET=%TARGET%
if not exist %TARGET% mkdir %TARGET%

if not x%1 == x set DEST=%1

:build
cd miniport
build -cZg
cd ../display
build -cZg
cd ../

:copy_local
copy display\obj%BUILD_ALT_DIR%\i386\qxldd.dll %TARGET%
copy miniport\obj%BUILD_ALT_DIR%\i386\qxl.sys %TARGET%
copy miniport\qxl.inf %TARGET%
copy display\obj%BUILD_ALT_DIR%\i386\qxldd.pdb %TARGET%
copy miniport\obj%BUILD_ALT_DIR%\i386\qxl.pdb %TARGET%
if not defined DEST goto exit
if exist %DEST% (
 echo copying to %DEST%
 if not exist %DEST%\%TARGET% ( mkdir "%DEST%\%TARGET%" )
 xcopy /s /y %TARGET% %DEST%\%TARGET%
)

:exit

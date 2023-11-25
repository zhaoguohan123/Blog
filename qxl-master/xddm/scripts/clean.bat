REM Copyright Red Hat 2009-2011
REM Authors: Arnon Gilboa 
:rmdir /S /Q Debug
rmdir /S /Q Release
for /d %%a in (obj*) do rd /s /q "%%a"
del /F *.log *.wrn *.err

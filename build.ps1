$BUILD_DIR="$PSScriptRoot\build"
if (Test-Path $BUILD_DIR)
{
    Remove-Item -Recurse $BUILD_DIR/*
}
else
{
    mkdir $BUILD_DIR
}
cd $BUILD_DIR

cmake ..
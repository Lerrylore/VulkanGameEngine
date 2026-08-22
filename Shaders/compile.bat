@echo off
setlocal

if not defined VULKAN_SDK (
    echo VULKAN_SDK is not defined. Install or activate the Vulkan SDK.
    exit /b 1
)

pushd "%~dp0"

"%VULKAN_SDK%\Bin\slangc.exe" shader.slang ^
    -target spirv ^
    -profile spirv_1_4 ^
    -emit-spirv-directly ^
    -fvk-use-entrypoint-name ^
    -entry vertMain ^
    -entry shadowVertMain ^
    -entry fragMain ^
    -o slang.spv
if errorlevel 1 goto :compile_failed

"%VULKAN_SDK%\Bin\slangc.exe" particles.slang ^
    -target spirv ^
    -profile spirv_1_4 ^
    -emit-spirv-directly ^
    -fvk-use-entrypoint-name ^
    -entry particleVertMain ^
    -entry particleFragMain ^
    -o particles.spv
if errorlevel 1 goto :compile_failed

"%VULKAN_SDK%\Bin\slangc.exe" compute.slang ^
    -target spirv ^
    -profile spirv_1_4 ^
    -emit-spirv-directly ^
    -fvk-use-entrypoint-name ^
    -entry compMain ^
    -o compute.spv
if errorlevel 1 goto :compile_failed

set "compileResult=%errorlevel%"
popd

exit /b %compileResult%

:compile_failed
set "compileResult=%errorlevel%"
popd
exit /b %compileResult%

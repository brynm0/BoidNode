@echo off

REM Set the include directories
set INCLUDE=%INCLUDE%;C:\Users\Bryn\Desktop\Code\imnodes;C:\Users\Bryn\Desktop\Code\imgui;C:\Users\Bryn\Desktop\Code\imgui\backends;C:\Users\Bryn\Desktop\Code\glew-2.1.0\include;C:\Users\Bryn\Desktop\Code\libmorton\include\libmorton

REM Set the library directories
set LIB=%LIB%;C:\Users\Bryn\Desktop\Code\glew-2.1.0\lib\Release\x64

REM Debug: Output timestamps for verification
if exist imgui_wrapper.lib (
    for %%L in (imgui_wrapper.lib) do (
        echo imgui_wrapper.lib timestamp: %%~tL
        set LIB_TIMESTAMP=%%~tL
    )
    for %%F in (imgui_wrapper.cpp imgui_wrapper.h) do (
        echo %%F timestamp: %%~tF
    )
)

REM Check if imgui_wrapper.cpp or imgui_wrapper.h have been modified since imgui_wrapper.lib was last compiled
if exist imgui_wrapper.lib (
    for %%F in (imgui_wrapper.cpp imgui_wrapper.h) do (
        for %%L in (imgui_wrapper.lib) do if %%~tF GTR %%~tL (
            echo Recompiling imgui_wrapper.lib because %%F is newer.
            call compile_imgui.bat
            goto CompileMain
        )
    )
) else (
    echo imgui_wrapper.lib not found. Compiling...
    call compile_imgui.bat
)
REM  /O2 /Ox /fp:fast /arch:AVX2 /GL


:CompileMain
REM Compile the program, suppressing normal output and only showing errors and warnings
cl /nologo /EHsc /Zi /O2 /Ox /fp:fast /arch:AVX2 /GL /Femain.exe main.cpp ^
    /link /LIBPATH:C:\Users\Bryn\Desktop\Code\glew-2.1.0\lib\Release\x64 glew32s.lib opengl32.lib gdi32.lib dwmapi.lib user32.lib imgui_wrapper.lib
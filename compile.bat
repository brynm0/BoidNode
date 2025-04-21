@echo off

REM Set up symbol path for Address Sanitizer
set _NT_SYMBOL_PATH=srv*c:\symbols*https://msdl.microsoft.com/download/symbols
REM Create symbols directory if it doesn't exist
if not exist c:\symbols mkdir c:\symbols

REM Configure Address Sanitizer for proper symbolization
set ASAN_OPTIONS=symbolize=1
set ASAN_SYMBOLIZER_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.43.34808\bin\HostX64\x64\llvm-symbolizer.exe

REM Set the include directories
set INCLUDE=%INCLUDE%;C:\Users\Bryn\Desktop\Code\imnodes;C:\Users\Bryn\Desktop\Code\imgui;C:\Users\Bryn\Desktop\Code\imgui\backends;C:\Users\Bryn\Desktop\Code\glew-2.1.0\include;C:\Users\Bryn\Desktop\Code\libmorton\include\libmorton;

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
            goto CompileBoidWin32
        )
    )
) else (
    echo imgui_wrapper.lib not found. Compiling...
    call compile_imgui.bat
)

:CompileBoidWin32
REM Check if boid_win32.cpp has been modified since boid_win32.lib was last compiled
@REM if exist boid_win32.lib (
@REM     for %%F in (boid_win32.cpp boid_platform.h) do (
@REM         for %%L in (boid_win32.lib) do if %%~tF GTR %%~tL (
@REM             echo Recompiling boid_win32.lib because %%F is newer.
@REM             goto CompileBoidWin32Lib
@REM         )
@REM     )
@REM     echo boid_win32.lib is up to date.
@REM     goto CompileMain
@REM ) else (
@REM     echo boid_win32.lib not found. Compiling...
@REM     goto CompileBoidWin32Lib
@REM )

:CompileBoidWin32Lib
REM Compile boid_win32.cpp as a static library
echo Compiling boid_win32.lib...
cl /nologo /EHsc /Zi /c /O2 /Ox /fp:fast /arch:AVX2 /GL boid_win32.cpp
if %ERRORLEVEL% NEQ 0 (
    echo Error compiling boid_win32.cpp
    exit /b %ERRORLEVEL%
)
lib /nologo /OUT:boid_win32.lib boid_win32.obj
if %ERRORLEVEL% NEQ 0 (
    echo Error creating boid_win32.lib
    exit /b %ERRORLEVEL%
)
echo Successfully compiled boid_win32.lib

REM  /O2 /Ox /fp:fast /arch:AVX2 /GL
REM  /fsanitize=address
REM /DTRACY_ENABLE

:CompileMain
REM Compile the program, suppressing normal output and only showing errors and warnings
cl /nologo /EHsc /Zi  /O2 /Ox /fp:fast /arch:AVX2 /GL /Femain.exe main.cpp tracy\public\TracyClient.cpp ^
    /link /LIBPATH:C:\Users\Bryn\Desktop\Code\glew-2.1.0\lib\Release\x64 glew32s.lib opengl32.lib gdi32.lib dwmapi.lib user32.lib imgui_wrapper.lib boid_win32.lib
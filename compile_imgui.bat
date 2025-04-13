@echo off

REM Set the include directories
set INCLUDE=%INCLUDE%;C:\Users\Bryn\Desktop\Code\imnodes;C:\Users\Bryn\Desktop\Code\imgui;C:\Users\Bryn\Desktop\Code\imgui\backends;C:\Users\Bryn\Desktop\Code\glew-2.1.0\include

REM Set the library directories
set LIB=%LIB%;C:\Users\Bryn\Desktop\Code\glew-2.1.0\lib\Release\x64

REM Compile each source file separately
cl /c /EHsc /Zi imgui_wrapper.cpp /Foimgui_wrapper.obj
cl /c /EHsc /Zi C:\Users\Bryn\Desktop\Code\imgui\imgui.cpp /Foimgui.obj
cl /c /EHsc /Zi C:\Users\Bryn\Desktop\Code\imgui\imgui_draw.cpp /Foimgui_draw.obj
cl /c /EHsc /Zi C:\Users\Bryn\Desktop\Code\imgui\imgui_widgets.cpp /Foimgui_widgets.obj
cl /c /EHsc /Zi C:\Users\Bryn\Desktop\Code\imgui\imgui_tables.cpp /Foimgui_tables.obj
cl /c /EHsc /Zi C:\Users\Bryn\Desktop\Code\imgui\backends\imgui_impl_win32.cpp /Foimgui_impl_win32.obj
cl /c /EHsc /Zi C:\Users\Bryn\Desktop\Code\imgui\backends\imgui_impl_opengl3.cpp /Foimgui_impl_opengl3.obj

REM Link all object files into imgui_wrapper.lib
lib /OUT:imgui_wrapper.lib imgui_wrapper.obj imgui.obj imgui_draw.obj imgui_widgets.obj imgui_tables.obj imgui_impl_win32.obj imgui_impl_opengl3.obj
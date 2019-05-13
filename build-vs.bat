@del /q *~ bin\*.dll bin\*.exe objs\*.exp 2> nul
@rmdir /s /q objs 2> nul
@if "%~1" == "clean" (exit /b 0)
@if "%~2" neq "" (echo Usage: %~n0 [clean] && exit /b 1)
@set CXXFLAGS=/nologo /W4 /EHsc /Foobjs\ /Febin\ /Ox
@set CPPFLAGS=/DUSE_SSE4 /DUSE_WINAPI /DNDEBUG /Isrc\common /Iwin\include
@mkdir objs

cl %CPPFLAGS% %CXXFLAGS% src\gencode\gencode.cpp
@if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

bin\gencode
@if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

cl %CPPFLAGS% %CXXFLAGS% src\autousi\autousi.cpp src\autousi\client.cpp src\autousi\pipe.cpp src\common\iobase.cpp src\common\option.cpp src\common\jqueue.cpp src\common\xzi.cpp src\common\err.cpp src\common\shogibase.cpp src\common\osi.cpp win\lib64\liblzma.lib Ws2_32.lib
@if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

lib /nologo /machine:x64 /def:win\def\OpenCL.def /out:objs\OpenCL.lib
@if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

@set CXXFLAGS=/nologo /W4 /EHsc /Foobjs\ /Febin\ /Ox
@set CPPFLAGS=/DNDEBUG /Isrc\common /Iwin\include
cl %CPPFLAGS% %CXXFLAGS% src\ocldevs\ocldevs.cpp src\common\err.cpp objs\OpenCL.lib

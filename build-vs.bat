@del /q *~ bin\*.dll bin\*.exe objs\*.exp 2> nul
@rmdir /s /q objs 2> nul
@if "%~1" == "clean" (exit /b 0)
@if "%~2" neq "" (echo Usage: %~n0 [clean] && exit /b 1)
@set CXXFLAGS=/nologo /W4 /EHsc /Foobjs\ /Febin\ /Ox /fp:fast
@set CPPFLAGS=/DUSE_SSE4 /DUSE_MKL /DUSE_WINAPI /DUSE_OPENCL /DNDEBUG /Isrc\common /Iwin\include
@set LDFLAGS=/link /LIBPATH:win\lib64
@mkdir objs

lib /nologo /machine:x64 /def:win\def\OpenCL.def /out:objs\OpenCL.lib
@if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

copy win\lib64\libiomp5md.dll bin\
cl %CPPFLAGS% %CXXFLAGS% src\gencode\gencode.cpp
@if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

bin\gencode
@if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

cl %CPPFLAGS% %CXXFLAGS% src\net-test\net-test.cpp src\common\opencl.cpp src\net-test\nnet.cpp src\net-test\nnet-cpu.cpp src\net-test\nnet-ocl.cpp src\common\err.cpp src\common\iobase.cpp src\common\shogibase.cpp src\common\xzi.cpp src\common\osi.cpp src\common\option.cpp %LDFLAGS% Ws2_32.lib liblzma.lib mkl_intel_lp64.lib mkl_intel_thread.lib mkl_core.lib libiomp5md.lib objs\OpenCL.lib
@if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

rem cl %CPPFLAGS% %CXXFLAGS% src\autousi\autousi.cpp src\autousi\client.cpp src\autousi\pipe.cpp src\common\iobase.cpp src\common\option.cpp src\common\jqueue.cpp src\common\xzi.cpp src\common\err.cpp src\common\shogibase.cpp src\common\osi.cpp %LDFLAGS% Ws2_32.lib liblzma.lib
rem @if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

cl %CPPFLAGS% %CXXFLAGS% src\ocldevs\ocldevs.cpp src\common\err.cpp src\common\opencl.cpp %LDFLAGS% objs\OpenCL.lib
@if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

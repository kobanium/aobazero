@rem Uncomment if you want to enable assersions
rem set ENABLE_ASSERT_AOBA=1

@rem Uncomment if you want to use OpenCL
set USE_OpenCL_AOBA=1

@rem uncomment if you want to use IntelMKL
rem set USE_CPUBLAS_AOBA=IntelMKL

@rem uncomment if you want to use OpenBLAS
set USE_CPUBLAS_AOBA=OpenBLAS

@rem uncomment if you want to use POLICY2187
set USE_POLICY2187=1

@cd /d %~dp0
@del /q *~ bin\*.dll bin\*.exe src\common\tbl_*.inc 2> nul
@rmdir /s /q objs 2> nul
@if "%~1" == "clean" (exit /b 0)
@if "%~2" neq "" (echo Usage: %~n0 [clean] && exit /b 1)

@mkdir objs

@set CXXFLAGS=/nologo /W4 /EHsc /Foobjs\ /Febin\ /Ox /fp:fast
@set CPPFLAGS=/DUSE_SSE4 /DUSE_WINAPI /Isrc\common /Iwin\include
@set LDFLAGS=/link /LIBPATH:win\lib64 liblzma.lib
@if "%ENABLE_ASSERT_AOBA%"=="1" (
    set CPPFLAGS=%CPPFLAGS% /DDEBUG
) else (
  set CPPFLAGS=%CPPFLAGS% /DNDEBUG
)
@if "%USE_OpenCL_AOBA%"=="1" (
    set CPPFLAGS=%CPPFLAGS% /DUSE_OPENCL_AOBA
    set LDFLAGS=%LDFLAGS% objs\OpenCL.lib
    lib /nologo /machine:x64 /def:win\def\OpenCL.def /out:objs\OpenCL.lib
    @if %ERRORLEVEL% neq 0 (exit /b %ERRORLEVEL%)
)
@if "%USE_CPUBLAS_AOBA%"=="IntelMKL" (
    set CPPFLAGS=%CPPFLAGS% /DUSE_MKL /openmp
    set LDFLAGS=%LDFLAGS% mkl_intel_lp64.lib mkl_intel_thread.lib mkl_core.lib libiomp5md.lib
    copy win\lib64\libiomp5md.dll bin\
    @if %ERRORLEVEL% neq 0 (exit /b %ERRORLEVEL%)
)
@if "%USE_CPUBLAS_AOBA%"=="OpenBLAS" (
    set CPPFLAGS=%CPPFLAGS% /DUSE_OPENBLAS /openmp
    set LDFLAGS=%LDFLAGS% libopenblas.dll.a
    copy win\lib64\libopenblas.dll bin\
    copy win\lib64\libgcc_s_seh-1.dll bin\
    copy win\lib64\libgfortran-3.dll bin\
    copy win\lib64\libquadmath-0.dll bin\
    @if %ERRORLEVEL% neq 0 (exit /b %ERRORLEVEL%)
)
@if "%USE_POLICY2187%"=="1" (
    set CPPFLAGS=%CPPFLAGS% /DUSE_POLICY2187
)

cl %CPPFLAGS% %CXXFLAGS% src\gencode\gencode.cpp
@if %ERRORLEVEL% neq 0 (exit /b %ERRORLEVEL%)

bin\gencode
@if %ERRORLEVEL% neq 0 (exit /b %ERRORLEVEL%)

cl %CPPFLAGS% %CXXFLAGS% src\net-test\net-test.cpp src\common\opencli.cpp src\common\nnet.cpp src\common\nnet-cpu.cpp src\common\nnet-ocl.cpp src\common\err.cpp src\common\iobase.cpp src\common\shogibase.cpp src\common\xzi.cpp src\common\osi.cpp src\common\option.cpp Ws2_32.lib %LDFLAGS% 
@if %ERRORLEVEL% neq 0 (exit /b %ERRORLEVEL%)

cl %CPPFLAGS% %CXXFLAGS% src\ocldevs\ocldevs.cpp src\common\err.cpp src\common\opencli.cpp %LDFLAGS%
@if %ERRORLEVEL% neq 0 (exit /b %ERRORLEVEL%)

cl %CPPFLAGS% %CXXFLAGS% src\autousi\autousi.cpp src\autousi\play.cpp src\autousi\client.cpp src\common\child.cpp src\common\opencli.cpp src\common\nnet.cpp src\common\nnet-cpu.cpp src\common\nnet-ocl.cpp src\common\nnet-srv.cpp src\common\jqueue.cpp src\common\err.cpp src\common\iobase.cpp src\common\shogibase.cpp src\common\xzi.cpp src\common\osi.cpp src\common\option.cpp Ws2_32.lib %LDFLAGS% 
@if %ERRORLEVEL% neq 0 (exit /b %ERRORLEVEL%)

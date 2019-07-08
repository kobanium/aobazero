How to compile
  ubuntu

    # clinfo install
    sudo apt install clinfo && clinfo

    # library install
    sudo apt install libboost-dev libboost-program-options-dev libboost-filesystem-dev opencl-headers ocl-icd-libopencl1 ocl-icd-opencl-dev zlib1g-dev

    # make
    make

    For CPU version, comment out Makefile line 53 with "#",
     and delete line 54's "#". Then -DUSE_CPU_ONLY is defined.


  Visual Studio 2017
    open "msvc/aoba-zero2017.sln".
    If you see "There is no Windows SDK version 10.0.17763.0.", 
     select "aoba-zero project", "property", "general", "WindowsSDK version",
     and change your install version.
    CPU version,
      Select "aoba-zero project", "property", "C/C++", "preprocessor","difine preprocessor",
      and add ";USE_CPU_ONLY" to the last.


aobaz command line option

  -p arg           number of playouts.
  -w arg           File with network weights.
  -q               Disable all diagnostic output.
  -u arg           ID of the OpenCL device(s) to use (disables autodetection).
  -i               Send information while thinking. Like,
                   "info depth 1 score cp -40 nodes 1 nps 333 pv 2g2f"


Self-play options:
  -n                Enable policy network randomization.
  -m arg (=0)       Play more randomly the first x moves.
  -mtemp arg (=1)   Visit count sampling temperature.


e.g.
  ./aobaz -p 800 -q -i -n -m 30 -w weight_save/w000000000465.txt

  aobaz plays same move without "-n" and "-m" option.



Network weight file
  You can get latest weight file by running "./bin/autousi".
  Weight files are stored in "weight_save".
  Past weights.
  https://drive.google.com/drive/folders/1yXO1ml0fYWL8bq3tTMOMVijG4Wd2-5FL
  Past game records.
  https://drive.google.com/drive/folders/1dbE5xWGQLsduR00oxEGPpZQJtQr_EQ75


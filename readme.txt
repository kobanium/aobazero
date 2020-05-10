1. AobaZero が提供するプログラム群のビルド方法
---------------------------------------------

Ubuntu, CentOS, Windows でビルドする方法を記します。OpenCL 1.2 の開発・実行
環境か、CPU 上で実行可能な BLAS (IntelMKL か OpenBLAS) が必要になります。両
方使うことも可能です。

1.1 Ubuntu

Ubuntu での手順を記します。ビルド前に、利用したいハードウエアのベンダが提供
する情報に従って、OpenCL 1.2 の環境を各自ご用意下さい。CPU のみで計算を行う
プログラムをビルドする場合には OpenCL 環境は不要です。

- Boost や liblzma 等をインストール

> sudo apt install libboost-dev libboost-program-options-dev
> sudo apt install libboost-filesystem-dev liblzma-dev zlib1g-dev
> sudo apt install opencl-headers ocl-icd-libopencl1 ocl-icd-opencl-dev

- make を実行

> make -j[number of threads]

もしCPUのみで計算を行うプログラムをビルドしたいならば、src/usi_engine/Makefile
の53行目を # でコメントし、54行目の # を消して、-DUSE_CPU_ONLY を有効にしてビ
ルドして下さい。


1.2 CentOS

CentOS での手順を記します。

- liblzma などをインストール

> sudo yum install xz-devel zlib-devel

- GCC 7.3.1 をインストール

> sudo yum -y install centos-release-scl
> sudo yum -y install devtoolset-7
> scl enable devtoolset-7 bash

GCC 7.3.1 が利用可能な環境で bash が起動します。

- boost 1.58.0 をインストール

> cd
> wget http://sourceforge.net/projects/boost/files/boost/1.58.0/boost_1_58_0.tar.gz
> gzip -dc boost_1_58_0.tar.gz | tar xvf -
> cd boost_1_58_0
> ./bootstrap.sh
> ./b2 install -j[number of threads] --prefix=/[a path with write permission]/inst-dts7

- OpenBLAS を使うならばこれをインストール

> cd
> git clone https://github.com/xianyi/OpenBLAS.git
> cd OpenBLAS
> make -j[number of threads]
> make install PREFIX=/[a path with write permission]/inst-dts7

- Intel MKL を使うならばこれをインストール

Intel 社が提供する情報に従ってインストールして下さい。

- OpenCL を使うならばこれの実行環境をインストール

利用したいハードウエアのベンダが提供する情報に従って、OpenCL 1.2 の環境を
インストールする。

- 環境変数の設定

コンンパイラとリンカのパスを設定します。

Bourne Shell 系ならば
> export CPLUS_INCLUDE_PATH=/path to inst-dts4/include:$CPLUS_INCLUDE_PATH
> export LIBRARY_PATH=/path to inst-dits4/lib:$LIBRARY_PATH
> export LD_RUN_PATH=/path to inst-dits4/lib:$LD_RUN_PATH

C Shell 系ならば
> setenv CPLUS_INCLUDE_PATH /path to inst-dts4/include:$CPLUS_INCLUDE_PATH
> setenv LIBRARY_PATH /path to inst-dits4/lib:$LIBRARY_PATH
> setenv LD_RUN_PATH /path to inst-dits4/lib:$LD_RUN_PATH

これらに加えて、OpenCL やIntel MKL などのパスの環境変数も必要ならば設定して
下さい。OpenCL, OpenBLAS 及び Intel MKL のパスは Makefile.config でも指定可
能です。

- make を実行

> make -j[number of threads]

もし CPU のみで計算を行う bin/aobaz をビルドしたいならば、
src/usi_engine/Makefile の 52 行目を # でコメントし、54 行目の # を消して、
-DUSE_CPU_ONLY を有効にしてビルドして下さい。


1.3 Windows 

- autousi.exe のビルド

Visual Studio 2017の「VS2017用 x64 Native Toolsコマンドプロンプト」を起動し、
バッチファイルを実行します。

> build_vs.bat

- aobaz.exe のビルド

まず、上述の autousi.exe のビルドを済ませてください。これによって生成される
幾つかのインクルードファイルが必要になります。そして、Visual Studio 2017
src/usi_engine/msvc/aoba-zero2017.sln を開き、構成マネージャーで構成を
Release、プラットフォームを x64 に設定しビルドします。「Windows SDK バージョ
ン 10.0.17763.0 が見つかりません」のエラーメッセージが出る場合はプロジェクト
プロパティの「全般」カテゴリの WindowsSDK バージョンを適切なバージョンに変更
します。

もしCPUのみで計算を行うプログラムをビルドしたいならば、プロジェクトプロパティ
の「C/C++、プリプロセッサ」カテゴリのプリプロセッサの定義で「;USE_CPU_ONLY」を
最後に追加します。


2. 計算資源を AobaZero のプロジェクトに提供する方法
---------------------------------------------------

Linux で棋譜を生成し、これを AobaZero のプロジェクトに提供する方法をここに記し
ます。Windows での方法は、バイナリ版の方法を参照下さい。

- OpenCL が認識するデバイスの確認

> sudo apt install clinfo && clinfo

- 使用するデバイス番号を設定

テキストエディタで autousi.cfg の Device の行を編集します。

- autousi を実行

> bin/autousi

ニューラルネットワークの重みファイルのダウンンロードが始まります。保存先は
weight_save です。これが終りしだい、autousi.cfg で指定した番号のデバイスを使用
して棋譜の生成を開始します。

ご協力ありがとうございます。

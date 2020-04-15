1. AobaZero のビルド方法
------------------------

Ubuntu, CentOS, Windows でビルドする方法を記します。ビルド前に、利用したいハー
ドウエアのベンダが提供する情報に従って、OpenCL 1.2 の環境を各自ご用意下さい。
CPU のみで計算を行うプログラムをビルドする場合には OpenCL 環境は不要です。


1.1 Ubuntu

Ubuntu での手順を記します。

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

- GCC 5.3.1 をインストール

> sudo yum -y install centos-release-scl
> sudo yum -y install devtoolset-4
> scl enable devtoolset-4 bash

GCC 5.3.1 が利用可能な環境で bash が起動します。

- boost 1.58.0 をインストール

> cd
> wget http://sourceforge.net/projects/boost/files/boost/1.58.0/boost_1_58_0.tar.gz
> gzip -dc boost_1_58_0.tar.gz | tar xvf -
> cd boost_1_58_0
> ./bootstrap.sh
> ./b2 install -j[number of threads] --prefix=/[a path with write permission]/inst-dts4

- OpenBLAS をインストール

> cd
> git clone https://github.com/xianyi/OpenBLAS.git
> cd OpenBLAS
> make -j[number of threads]
> sudo make install PREFIX=/[a path with write permission]/inst-dts4

- 環境変数の設定

コンンパイラとリンカのパスを設定します。

Bourne Shell 系ならば
> exprot CPLUS_INCLUDE_PATH=/path to inst-dts4/include:$CPLUS_INCLUDE_PATH
> export LIBRARY_PATH=/path to inst-dits4/lib:$LIBRARY_PATH
> export LD_RUN_PATH=/path to inst-dits4/lib:$LD_RUN_PATH

C Shell 系ならば
> setenv CPLUS_INCLUDE_PATH /path to inst-dts4/include:$CPLUS_INCLUDE_PATH
> setenv LIBRARY_PATH /path to inst-dits4/lib:$LIBRARY_PATH
> setenv LD_RUN_PATH /path to inst-dits4/lib:$LD_RUN_PATH

必要であれば、各ベンダが提供する OpenCL 開発環境へのパスも同様に設定します。

- make を実行

> make -j[number of threads]

もしCPUのみで計算を行うプログラムをビルドしたいならば、
src/usi_engine/Makefile の53行目を # でコメントし、54行目の # を消して、
-DUSE_CPU_ONLY を有効にしてビルドして下さい。


1.3 Windows 

- autousi.exe のビルド

Visual Studio 2017の「VS2017用 x64 Native Toolsコマンドプロンプト」を起動し、
バッチファイルを実行します。

> build_vs.bat

- aobaz.exe のビルド

Visual Studio 2017 で src/usi_engine/msvc/aoba-zero2017.sln を開きビルドします。
「Windows SDK バージョン 10.0.17763.0 が見つかりません」のエラーメッセージが出
る場合はプロジェクトプロパティの「全般」カテゴリの WindowsSDK バージョンを適切
なバージョンに変更します。

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

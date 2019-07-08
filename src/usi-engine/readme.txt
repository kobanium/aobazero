コンパイルの仕方
  ubuntu の場合

    # clinfo の install
    sudo apt install clinfo && clinfo

    # 必要なライブラリをインストール
    sudo apt install libboost-dev libboost-program-options-dev libboost-filesystem-dev opencl-headers ocl-icd-libopencl1 ocl-icd-opencl-dev zlib1g-dev

    # 実行ファイルを作成
    make

    CPU版を作るには Makefile の53行目を#でコメントし、
    54行目の#を消して -DUSE_CPU_ONLY を有効にします。


  Visual Studio 2017
    msvc/aoba-zero2017.sln を開きます。
    「Windows SDK バージョン 10.0.17763.0 が見つかりません」と出る場合は
    aoba-zero のプロジェクトのプロパティ、全般、WindowsSDKバージョン、を
    インストールされているものに変更します。
    CPU版を作成するには
    プロジェクトのプロパティ、C/C++、プリプロセッサ、プリプロセッサの定義、で
    ;USE_CPU_ONLY
    を最後に追加します。



aobaz の起動オプション

  -p arg           1手にかけるplayoutの回数。
  -w arg           ネットワークのweightの重みファイル名
  -q               余計な情報の表示をしない
  -u arg           OpenCL デバイスのIDを指定。0から。なしで自動選択。
  -i               思考中に情報を返します。以下のような形式です。
                   「info depth 1 score cp -40 nodes 1 nps 333 pv 2g2f」


  自己対戦用のオプション:
  -n               Rootにノイズを加えて最善手以外も探索しやすくします。
  -m arg (=0)      初手から x 手まで訪問回数の割合でランダムに選択します。
  -mtemp arg (=1)  0で訪問回数最大、大きいほど回数に関係なく選びます。


  ネットワーク同士の強さを計る場合は、-n と -m 30 を同時につけるのを推奨します。
  ./aobaz -p 800 -q -n -m 30 -w weight_save/w000000000465.txt

  -n -m をつけない場合、同じ手しか指しません。



ネットワークの重みファイル
  最新のものは bin/autousi を実行すると weight_save/ の下に保存されます。
  過去のweightはこちらにあります。
  https://drive.google.com/drive/folders/1yXO1ml0fYWL8bq3tTMOMVijG4Wd2-5FL


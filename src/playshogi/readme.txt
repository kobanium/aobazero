playshogiは2つのusiプログラム同士を対戦させます。

1. 513手に達した将棋は自動的に引き分け
2. 宣言勝ちも自動的に引き分け(通常は条件を満たしてから"win"を送りますが
   条件を満たす手を指した瞬間にplayshogiが判定します)。27点法
3. AobaZeroを使う場合、同時に複数の対戦を走らせ、プロセス間バッチを組むことで高速化できます。
4. 棋譜は標準出力に出ます。
5. 互角局面集を使った対戦が可能です。順番はランダムです。"-n" でランダムなし
6. 互角局面集
  やねうら王互角局面集2025  23手目と31手目まで登録。後手から開始
  https://yaneuraou.yaneu.com/2025/07/29/yaneuraou-balanced-position-collection-2025/
  dlshogi 互角局面集     2021年 9月20日 5,247局面 36手まで。24手目から開始を推奨
  https://tadaoyamaoka.hatenablog.com/entry/2021/09/20/222018
  dlshogi 中盤互角局面集 2022年12月31日 8,187局面 32手から80手まで。ほぼ相居飛車
  https://tadaoyamaoka.hatenablog.com/entry/2022/12/31/114258
  たややん終盤互角局面集(taya80gokaku.sfen) 2021年 1,865局面 すべて80手目
  https://x.com/tayayan_ts/status/1428276505616941056?lang=nl
  互角局面集(2016年) 24手目まで。10,831局面
  https://yaneuraou.yaneu.com/2016/08/24/
  駒落ち(香、角、飛、2枚、4枚、6枚)の初期局面集。16手目まで
  https://github.com/yssaya/komaochi/tree/master/opening

7. 実行例

やねうら王互角局面集2025で800局対戦。1手40kと20k、2スレッド。"-0"が先手です。
./playshogi -rsm 800 -o ./2025_start_sfens_ply24.sfen -c /bin/bash -0 "./yane483 , usi , setoption name BookMoves value 0 , setoption Threads value 2 , setoption NodesLimit value 40000 , isready" -1 "./yane483 , usi , setoption name BookMoves value 0 , setoption Threads value 2 , setoption NodesLimit value 20000 , isready" >> A_40k_vs_B_20k_2t_2025_ply24.csa

AobaNNUEと振電3を互角局面集(2016年)で。ディレクトリが異なる。FV_SCALEを指定。0.1秒/手で(ConstantThinkingTime value 100、はAobaNNUEのソースでのみ有効)
./playshogi -rsm 800 -o ./records2016_10818.sfen -c /bin/bash -0 "cd ~/aobannue/; ./yane900zen3_768_16_64 , setoption name BookMoves value 0 , setoption name Threads value 8 , setoption name ConstantThinkingTime value 100 , setoption name FV_SCALE value 40 , isready" -1 "cd ../sinden3; ./yane900zen3_512_8_64 , setoption name BookMoves value 0 , setoption name Threads value 8 , setoption name ConstantThinkingTime value 100 , setoption name FV_SCALE value 40 , isready" >> 768_900zen3FV40_vs_sn3_900zen3FV40_8t_100ms.csa

Aoba駒落ちの2枚落ち初期局面集で対戦。player0が常に先手
./playshogi -frsm 400 -o ./komaochi/opening/20211003_2mai.sfen -c /bin/bash -0 "./yane483 , usi , setoption name BookMoves value 0 , setoption Threads value 1 , setoption NodesLimit value 40000 , isready" -1 "./yane483 , usi , setoption name BookMoves value 0 , setoption Threads value 1 , setoption NodesLimit value 40000 , isready" >> A_vs_B_2mai_40k.csa

AobaZero同士を対戦させる場合。800局。互角定跡集を400局使って先後交互に。
./playshogi -rsm 800 -o ./records2016_10818.sfen -0 "./bin/aobaz -p 100 -w ./weight/w1198.txt" -1 "./bin/aobaz -p 100 -w ./weight/w1198.txt" >> w1198_p100_vs_w1198_p100.csa

AobaZero(1手800playout)とKristallweizen(1手200kノード、1スレッド、定跡なし)を対戦させる場合。プロセス間バッチ利用。HALF利用。weightの指定はplayshogi、aobaz、同じものを指定してください(内部で時々GPUの計算とCPUの計算の一致を確認するため)。
./playshogi -rsbm 600 -B 7 -P 25 -U 0 -H 1 -c /bin/bash -W ./weight/w1198.txt -0 "./bin/aobaz -p 800 -e 0 -w ./weight/w1198.txt" -1 "~/Kristallweizen/yane483_nnue_avx2 usi , setoption name BookMoves value 0 , setoption Threads value 1 , setoption USI_Hash value 16 , setoption NodesLimit value 200000 , isready" >> w1198_p800_vs_200k.csa

AobaZero(1手800playout)と水匠5(1手300kノード、1スレッド、定跡なし)を対戦させる場合。
./playshogi -rsbm 800 -P 18 -B 7 -U 0 -H 1 -c /bin/bash -W ./w4365.txt -0 "./aobazero/bin/aobaz -p 800 -e 0 -w ./w4365.txt" -1 "cd ../suisho5; ./yane750sse42 , isready , setoption name BookMoves value 0 , setoption Threads value 1 , setoption NodesLimit value 300000" >> w4365_s5_750_300k.csa

GPU 0 と GPU 1 を使ってw485とw450を800局対戦。定跡集は使わず。ノイズの追加と -msafe 30 で30手目まで勝率2%以内なら次善手でも選ぶ。
./playshogi -rsm 800 -P 25 -U 0:1 -B 7:7 -H 1:1 -W w0485.txt:w0450.txt -0 "bin/aobaz -e 0 -p 800 -n -msafe 30 -w w0485.txt" -1 "bin/aobaz -e 1 -n -msafe 30 -p 800 -w w0450.txt"

GPU 0 のみを用いてw1650とw1500を対戦。
./playshogi -brsm 800 -P 18 -B 7:7 -U 0:0 -H 1:1 -c /bin/bash -W ./w1650.txt:./w1500.txt w -0 "bin/aobaz -p 800 -e 0 -w w1650.txt" -1 "bin/aobaz -p 800 -e 1 -w w1500.txt" >> w1650_vs_w1500.csa

dlshogiと1手100playoutで。
.playshogi -brsm 800 -i usi_dummy.txt:usi_dr2_mb1_p100.txt -c /bin/bash -P 7 -U 0 -B 3 -H 1 -W w4357.txt -0 "./aobaz -p 100 -e 0 -w w4357.txt" -1 "dlshogi_dr2_exhi/usi/bin/usi" >> w4357_vs_dlshogi_dr2_100p.csa
$ cat usi_dr2_mb1_p100.txt
setoption name DNN_Model value /home/yss/shogi/dlshogi_dr2_exhi/model/model-dr2_exhi.onnx
setoption name DNN_Batch_Size value 1
setoption name Const_Playout value 100

"-i" で usi コマンドを送れますが、usi と isready の間の setoption のみを指定します。
指定する場合は必ず2つ必要です。送る必要がない場合は 改行1行だけの usi_dummy.txt を指定します。

usi
...
usiok

  ここで指定する setoption を複数行で指定

isready



8. 注意
  ubuntu 16だと "-c /bin/bash" を付けないとAobaZeroのプロセス間バッチは動作しません。
  CentOSだと必要ないです。これは "sh -c" で起動したプロセスがubuntuだと子プロセスでなく孫プロセスになるためです。
  同一ディレクトリで複数のplayshogiは動きません。

9. 結果の見方
   W-D-L    Games(DW-rep-DL) Sente WinR                WinRate 95%   ELO
   勝 分 敗 局数 (宣 千 宣)    先手勝率                 勝率   95%   ELO
  437-13-350 800 (50-10-2)(s=422-365,0.536), m=133, wr=0.554(0.034)(  37)

  (50-10-2) は先手の宣言勝ちが50局、千日手の引き分けが10局、後手の宣言勝ちが2局、です。
  513手超えは13-10=3局です。95%は信頼区間です。
  勝率は95%で 0.554 - 0.034 < wr < 0.554 + 0.034 の間に入る、という意味です。
  0.554 - 0.034 > 0.50 なので95%の確率で有意に強い、とも言えます。



Usage: playshogi [OPTION] -0 "CMD0" -1 "CMD1"

      Generate gameplays between two USI shogi engines

Mandatory options:
  -0 "CMD0" Start player0 as '/bin/sh -c "CMD0"'.
  -1 "CMD1" Start player1 as '/bin/sh -c "CMD1"'.

Other options:
  -m NUM   Generate NUM gameplays. NUM must be a positive integer. The default
           value is 1.
  -f       Always assign player0 to Sente (black). If this is not specified,
           then Sente and Gote (white) are assigned alternatively.
  -r       Print CSA records.
  -s       Print results in detail.
  -u       Print verbose USI messages.
  -b       Use positions recorded in records2016_10818.sfen (a collection of
           24 moves from the no-handicap initial position).
  -o STR   Use sfen positions file. Like 'start_sfens_ply32.sfen'
  -n       Do not shuffle sfen positions file.
  -v       Use 'go visit' to get aobak 'v=' and searched moves and nodes.
  -y STR   Use 'go btime 0 wtime 0 byoyomi 3000'. STR=3000
  -c SHELL Use SHELL, e.g., /bin/csh, instead of /bin/sh.
  -P NUM   Generate NUM gameplays simultaneously. The default is 1.
  -I STR   Specifies nnet implementation. STR can conatin two characters
           separated by ':'. Character 'B' means CPU BLAS implementation, and
           'O' means OpenCL implimentation. The default is 'O'.
  -B STR   Specifies batch sizes of nnet computation. STR can contain two
           sizes separated by ':'. The default size is 1.
  -W STR   Specifies weight paths for nnet computation. STR can contain two
           file names separated by ':'.
  -U STR   Specifies device IDs of OpenCL nnet computation. STR can contain
           two IDs separated by ':'. Each ID must be different from the other.
  -H STR   OpenCL uses half precision floating-point values. STR can contain
           two binary values separated by ':'. The value should be 1 (use
           half and NVIDIA wmma instructions if possible), or 0 (do not use
           half). The default is 0.
  -T STR   Specifies the number of threads for CPU BLAS computation. STR can
           contain two numbers separated by ':'. The default is -1 (means an
           upper bound of the number).
  -i STR   usi option file paths. STR must contain two
           sizes separated by ':'.
Example:
  playshogi -0 "~/aobaz -w ~/w0.txt" -1 "~/aobaz -w ~/w1.txt"
           Generate a gameplay between 'w0.txt' (black) and 'w1.txt' (white)


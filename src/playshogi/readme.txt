playshogiは2つのusiプログラム同士を対戦させます。

1. 513手に達した将棋は自動的に引き分け
2. 宣言勝ちも自動的に引き分け(通常は条件を満たしてから"win"を送りますが
   条件を満たす手を指した瞬間にplayshogiが判定します)。27点法
3. AobaZeroを使う場合、同時に複数の対戦を走らせ、プロセス間バッチを組むことで高速化できます。
4. 棋譜は標準出力に出ます。
5. 磯崎氏が作成された24手目までの互角局面集を使って対戦できます。
   http://yaneuraou.yaneu.com/2016/08/24/%E8%87%AA%E5%B7%B1%E5%AF%BE%E5%B1%80%E7%94%A8%E3%81%AB%E4%BA%92%E8%A7%92%E3%81%AE%E5%B1%80%E9%9D%A2%E9%9B%86%E3%82%92%E5%85%AC%E9%96%8B%E3%81%97%E3%81%BE%E3%81%97%E3%81%9F/
   records2016_10818.sfen
   をカレントディレクトリに置いて下さい。
   局面数は起動ごとにランダムに選ばれます。


例:
AobaZero同士を対戦させる場合。800局。互角定跡集を400局使って先後交互に。"-0"が先手です。
bin/playshogi -rsbm 800 -0 "./bin/aobaz -p 100 -w ./weight/w1198.txt" -1 "./bin/aobaz -p 100 -w ./weight/w1198.txt" >> w1198_p100_vs_w1198_p100.csa

AobaZero(1手800playout)とKristallweizen(1手200kノード、1スレッド、定跡なし)を対戦させる場合。プロセス間バッチ利用。HALF利用。weightの指定はplayshogi、aobaz、同じものを指定してください(内部で時々GPUの計算とCPUの計算の一致を確認するため)。
bin/playshogi -rsbm 600 -B 7 -P 25 -U 0 -H 1 -c /bin/bash -W ./weight/w1198.txt -0 "./bin/aobaz -p 800 -e 0 -w ./weight/w1198.txt" -1 "~/Kristallweizen/yane483_nnue_avx2 usi , setoption name BookMoves value 0 , setoption Threads value 1 , setoption USI_Hash value 16 , setoption NodesLimit value 200000 , isready" >> w1198_p800_vs_200k.csa

AobaZero(1手800playout)と水匠5(1手300kノード、1スレッド、定跡なし)を対戦させる場合。
bin/playshogi -rsbm 800 -P 18 -B 7 -U 0 -H 1 -c /bin/bash -W ./w4365.txt -0 "./aobazero/bin/aobaz -p 800 -e 0 -w ./w4365.txt" -1 "cd ../suisho5; ./yane750sse42 , isready , setoption name BookMoves value 0 , setoption Threads value 1 , setoption NodesLimit value 300000" >> w4365_s5_750_300k.csa

GPU 0 と GPU 1 を使ってw485とw450を800局対戦。定跡集は使わず。ランダム性としてノイズの追加と最初の30手は確率分布で選択。
bin/playshogi -rsm 800 -P 25 -U 0:1 -B 7:7 -H 1:1 -W w0485.txt:w0450.txt -0 "bin/aobaz -e 0 -p 800 -n -m 30 -w w0485.txt" -1 "bin/aobaz -e 1 -n -m 30 -p 800 -w w0450.txt"

GPU 0 のみを用いてw1650同士を対戦。片方は -msafe 30 で30手目まで乱数性を持たせる。
bin/playshogi -rsm 800 -P 25 -B 7 -U 0 -H 1 -c /bin/bash -W w1650.txt -0 "bin/aobaz -p 800 -msafe 30 -e 0 -w w1650.txt" -1 "bin/aobaz -p 800 -e 0 -w w1650.txt"

2枚落ち。先手が常に下手。"-d 1" は香落ち、以下、角(2)、飛(3)、2枚(4)、4枚(5)、6枚(6)
bin/playshogi -frsm 800 -d 4 -0 "bin/aobaz -p 400 -msafe 30 -w w1525.txt" -1 "bin/aobaz -p 10 -msafe 30 -w w1525.txt" >> 2mai_p400_vs_p10.csa

dlshogiと1手100playoutで。
bin/playshogi -brsm 800 -i usi_dummy.txt:usi_dr2_mb1_p100.txt -c /bin/bash -P 7 -U 0 -B 3 -H 1 -W w4357.txt -0 "./aobaz -p 100 -e 0 -w w4357.txt" -1 "dlshogi_dr2_exhi/usi/bin/usi" >> w4357_vs_dlshogi_dr2_100p.csa
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



※ 注意
ubuntu 16だと "-c /bin/bash" を付けないとAobaZeroのプロセス間バッチは動作しません。
CentOSだと必要ないです。これは "sh -c" で起動したプロセスがubuntuだと子プロセスでなく孫プロセスになるためです。
同一ディレクトリで複数のplayshogiは動きません。


結果の見方
   W-D-L    Games(DW-rep-DL) Sente WinR                WinRate 95%   ELO
   勝 分 敗 局数 (宣 千 宣)    先手勝率                 勝率   95%   ELO
  437-13-350 800 (50-10-2)(s=422-365,0.536), m=133, wr=0.554(0.034)(  37)

  (50-10-2) は先手の宣言勝ちが50局、千日手の引き分けが10局、後手の宣言勝ちが2局、です。
  513手超えは13-10=3局です。








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
  -c SHELL Use SHELL, e.g., /bin/csh, instead of /bin/sh.
  -P NUM   Generate NUM gameplays simultaneously. The default is 1.
  -I STR   Specifies nnet implementation. STR can conatin two characters
           separated by ':'. Character 'B' means CPU BLAS implementation, and
           'O' means OpenCL implimentation. The default is 0.
  -B STR   Specifies batch sizes of nnet computation. STR can contain two
           sizes separated by ':'. The default size is 1.
  -W STR   Specifies weight paths for nnet computation. STR can contain two
           file names separated by ':'.
  -U STR   Specifies device IDs of OpenCL nnet computation. STR can contain
           two IDs separated by ':'. Each ID must be different from the other.
  -H STR   OpenCL uses half precision floating-point values. STR can contain
           two binary values separated by ':'. The value should be 1 (use
           half) or 0 (do not use half). The default is 0.
  -T STR   Specifies the number of threads for CPU BLAS computation. STR can
           contain two numbers separated by ':'. The default is -1 (means an
           upper bound of the number).
  -i STR   USI setoption file. STR can contain two numbers separated by ':'.
Example:
  playshogi -0 "~/aobaz -w ~/w0.txt" -1 "~/aobaz -w ~/w1.txt"
           Generate a gameplay between 'w0.txt' (black) and 'w1.txt' (white)


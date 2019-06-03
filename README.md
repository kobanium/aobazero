The text written in English is [here](README_en.md).
# おねがい
みなさん！将棋人工知能(AI)がゼロから学習し、強くなっていく過程を一緒に観察しませんか？  
世界中から計算資源を獲得するために、ユーザ参加型のコンテンツを作成しました。  
皆さま、オラたちに力をわけてくれっ！  

集めた棋譜はAobaZeroの[ウェブページ](http://www.yss-aya.com/aobazero/)などで公開しています。  
GPUがあれば、より高速に棋譜を生成できます。
CPUだと10倍から100倍遅くなりますが、将棋をプレイして楽しむことは可能です。

# 重要なニュース

release 1.2 では非常に重要な不具合修正がbin/aobazになされました。アップデートをお願い致します。


# AobaZero

AobaZeroは、AlphaZeroの将棋の実験の追試を行うことを最終目的とした将棋AIプロジェクトです。
AlphaZeroは、Silverら（2017b, 2018）が発表した強化学習のアルゴリズムの一種であり、
AIプレイヤの名前でもあります。
彼らが報告した将棋での実験結果は、ゲームルール以外の知識をほとんど持たないプレイヤが自己と対局し続けて、AIプレイヤelmoよりも強くなるというものでした。
これは非常に規模の大きな実験であり、追試を行うことは容易ではありません。
高性能なデスクトップゲーミングマシン1台で100年近い計算時間を要します。  
  
Silverら（2017a, 2018）は囲碁やチェスでの実験結果も報告しています。
ユーザ参加型の追試実験は、囲碁の[Leela Zero](https://zero.sjeng.org)
やチェスの[LCZero](https://lczero.org)によって行われていて、
既に人間を超える棋力を獲得するということが再現されています。

# 棋譜の生成に協力してみたい
[Windows用の実行ファイル(現在は64bit版のみです)](https://github.com/kobanium/aoba-zero/releases)

CPUだけのマシンは
```
aobazero-1.0-w64-cpu-only.zip
```
GPUがついたマシンは
```
aobazero-1.0-w64-opencl.zip
```
をダウンロード、展開して、中のclick_me.batを実行してください。

Linuxの方は
```
aobazero-1.0.tar.gz
```
を展開してmakeしてから
```
./bin/autousi
```
を実行してください。詳しくは同梱のreadme.txtをご覧ください。

# 将棋所で遊んでみたい
CPU版をダウンロードして、click_me.batを実行します。しばらくすると最新のネットワークの重みファイルをダウンロードして「PI」が表示されて棋譜の生成を開始します。すかさずCtrl + Cで停止させます。(signal 1 caught)が表示されて、しばらく待つと止まります。  
weight_save/の下にw000000000468.txt という230MBほどのファイルが作られます。
(468、の数値は異なります)

aobazero-1.0-w64-cpu-only.zipに同梱されているaobaz.batを編集します。最後の1行が以下のようになっています。
```
bin/aobaz -q -p 30 -w weight_save\w000000000467.txt
```
この467の部分を実際にダウンロードしてきたファイル名に合わせて書き直し、保存します。  
将棋所にaobaz.batをエンジンとして登録します。  
"-p 30"の30を増やすと強くなりますが、思考時間が長くなります。  
CPU版は30で5秒ほどかかります。GPU版は800で3秒ほどかかります(GPUの性能に依存します)。  
  
将棋所はusiエンジンを動作させる将棋用のGUIです。こちらで入手できます。  
将棋所のページ<http://shogidokoro.starfree.jp/>

# AobaZeroの紹介ページ
今までに作成した棋譜や重み、棋譜のサンプルなどを公開しています。  
<http://www.yss-aya.com/aobazero/>

# License
usiエンジンであるaobazはGPL v3です。それ以外はpublic domainです。  
詳しくはaobazero-1.0.tar.gz内のlicensesをご覧ください。

# Link
 - [Leela Zero (Go)](https://github.com/leela-zero/leela-zero)
 - [LCZero (Chess)](https://github.com/LeelaChessZero/lczero)

# 参考文献
 - D. Silver, et al. (2017a). Mastering the game of Go without human knowledge, *Nature*, **550**, 354-359.
 - D. Silver, et al. (2017b).  Mastering Chess and Shogi by Self-Play with a General Reinforcement Learning Algorithm, arXiv:1712.01815.
 - D. Silver, et al. (2018). A general reinforcement learning algorithm that masters chess, shogi, and Go through self-play, *Science*, **362**, 1140-1144 ([a preprint version is avairable online](https://deepmind.com/documents/260/alphazero_preprint.pdf)).

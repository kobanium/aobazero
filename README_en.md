# We need your help.
Ladies and gentlemen! Would you like to observe the process of Shogi AI learning from scratch and getting stronger together?
We created user-participated content to gain computing resources from around the world.
Give us just a little of your Genki!

The collected games are published on the AobaZero [page](http://www.yss-aya.com/aobazero/index_e.html).
With a GPU, you can generate games faster.
It will be 10 to 100 times slower with CPU, but it is possible to play and enjoy Shogi.

# AobaZero
AobaZero is a Shogi AI project that aims to replicate AlphaZero Shogi experiment.
AlphaZero is one of reinforcement learning algorithm published by Silver et al. (2017b, 2018).
It is also the name of an AI player.
The result of the Shogi experiment they reported were that
 a player who had no knowledge except game rules continued to play with himself
 and became stronger than the AI player elmo.
This is a very large experiment, and it is not easy to replicate.
It takes 100 years of calculation time if you use only one high-performance desktop gaming machine.

Silver et al. (2017a, 2018) also reported the results of experiments with Go and Chess.
The user-participated replication study projects are on going in Go [Leela Zero](https://zero.sjeng.org)
and Chess [LCZero](https://lczero.org). Both projects have achieved super human strength.

# I'd like to cooperate with the generation of the game records.
[Executable file for Windows(only 64-bit version)](https://github.com/kobanium/aoba-zero/releases)

For machine without GPU
```
aobazero-1.5-w64-cpu-only.zip
```
For machine with GPU
```
aobazero-1.5-w64-opencl.zip
```
Download it, unzip, and run click_me.bat.

For Linux,
```
aobazero-1.5.tar.gz
```
Unzip it, make, then run
```
./bin/autousi
```
Please see readme.txt for details.

# I'd like to play with ShogiDokoro.
Download CPU version and run click_me.bat.
After a while, it downloads the latest network weight file, and "self-play start" is displayed, and self-play starts. Input "Ctrl + C" immediately. (signal 1 caught) is displayed and it will stop after a while.

weight_save/w0000000002184.txt will be created. Its size is about 230MB.
(the numbers "2184" will be different.)

Edit aobaz.bat that is in aobazero-1.5-w64-cpu-only.zip.
The last line is like this,
```
bin/aobaz -q -i -p 100 -w weight-save\w000000002184.txt
```
Rewrite this "2184" according to the file name actually downloaded, and save.
Register aobaz.bat as a engine in ShogiDokoro.
Increase by 100 of "-p 100", to get stronger. But it gets slower.
The CPU version takes about 5 seconds at 100. 
The GPU version takes about 3 seconds at 4000.(It depends on GPU.) 

ShogiDokoro is a GUI for USI engine.

ShogiDokoro 
<http://shogidokoro.starfree.jp/>

# I'd like to play with ShogiGUI
AobaZero also can be played with ShogiGUI. But it does not send sequences from initial position(position startpos moves ...),
 but position (position sfen ...). AobaZero'NN needs past 7 moves, so it is weaker than on ShogiDokoro.

ShogiGUI 
<http://shogigui.siganus.com/>

# AobaZero introduction page
There are game records, network weights, and some self-play game samples.
<http://www.yss-aya.com/aobazero/>

# License
USI engine aobaz belongs to GPL v3. Others are in the public domain.
Detail is in the licenses in aobazero-1.0.tar.gz.

# Link
 - [Leela Zero (Go)](https://github.com/leela-zero/leela-zero)
 - [LCZero (Chess)](https://github.com/LeelaChessZero/lczero)

# References
 - D. Silver, et al. (2017a). Mastering the game of Go without human knowledge, *Nature*, **550**, 354-359.
 - D. Silver, et al. (2017b). Mastering Chess and Shogi by Self-Play with a General Reinforcement Learning Algorithm, arXiv:1712.01815.
 - D. Silver, et al. (2018). A general reinforcement learning algorithm that masters chess, shogi, and Go through self-play, *Science*, **362**, 1140-1144 ([a preprint version is avairable online](https://deepmind.com/research/publications/general-reinforcement-learning-algorithm-masters-chess-shogi-and-go-through-self-play)).

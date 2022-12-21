from cshogi import *
import numpy as np
import math

import argparse

parser = argparse.ArgumentParser()
parser.add_argument('hcpe')
#parser.add_argument('psv')
args = parser.parse_args()

hcpes = np.fromfile(args.hcpe, dtype=HuffmanCodedPosAndEval)
#psvs = np.zeros(len(hcpes), PackedSfenValue)
move_sum = 0
res = [0,0,0]
s_max = -999999
s_min = +999999
board = Board()
for hcpe in hcpes:
#for hcpe in zip(hcpes):
    board.set_hcp(hcpe['hcp'])
#    board.to_psfen(psv['sfen'])
    score = hcpe['eval']
    move16 = hcpe['bestMove16']
    move_csa = move_to_csa(board.move_from_move16(move16))
    #move_num = hcpe['moveNum']
    result = hcpe['gameResult']
    # gameResult -> 0: DRAW, 1: BLACK_WIN, 2: WHITE_WIN
    #if board.turn == gameResult - 1:
    #    psv['game_result'] = 1
    #elif board.turn == 2 - gameResult:
    #    psv['game_result'] = -1
    move_sum += 1
    res[result] += 1
    if score > s_max:
        s_max = score
    if score < s_min:
        s_min = score
    print ("'move_sum=" +str(move_sum) +",result=" +str(result) +  ",move=" + move_csa + ",score=" + str(score))
    print (board)

    if (board.turn) == 0:
        sen = "+"
    else:
        sen = "-"
    v = 1.0 / (1.0 + math.exp(-score * 0.0013226))
    comment = f"v={v:.3f},"
    print (sen + move_csa + "," + comment)
    s_res = "%SENNICHITE"
    if result == 1:
        s_res = "%-ILLEGAL_ACTION"
    elif result == 2:
        s_res = "%+ILLEGAL_ACTION"
    print (s_res)
    print ("/")

#psvs.tofile(args.psv)
print ("'move_sum=" +str(move_sum) +",result=" +str(result) +  ",move=" + move_csa + ",score=" + str(score))
print ("'res[0]=" +str(res[0]) +",[1]=" +str(res[1]) + ",[2]=" + str(res[2]) + ",s_max="+str(s_max)+",s_min"+str(s_min))

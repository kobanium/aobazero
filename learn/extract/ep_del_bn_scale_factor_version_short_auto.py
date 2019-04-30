#http://stackoverflow.com/questions/31324739/finding-gradient-of-a-caffe-conv-filter-with-regards-to-input
#import os
#import numpy as np
#import sys

#from google.protobuf import text_format
import caffe
#from caffe.proto import caffe_pb2
import struct
import sys

args = sys.argv
print(args[1])
#sys.exit()

caffe.set_mode_cpu()
#net = caffe.Net("/home/yss/shogi/yssfish/aoba_zero_256x20b.prototxt","/home/yss/shogi/yssfish/20190419replay_lr001_wd00002_100000_1018000/_iter_36000.caffemodel",caffe.TEST);
net = caffe.Net("/home/yss/shogi/yssfish/aoba_zero_256x20b.prototxt",args[1],caffe.TEST);





#net = caffe_pb2.NetParameter()
#text_format.Merge(open("aya_m3_solver.prototxt").read(), net)


#solver = caffe.SGDSolver( "aya_m3_solver.prototxt" )
#net = solver.net

#print net.blobs
#print net.inputs  # >> ['data']
#print net.blobs['data']
#print [(k, v.data.shape) for k, v in net.blobs.items()]

#You will see a dictionary storing a "caffe blob" object for each layer in the net. Each blob has storing room for both data and gradient

#print net.blobs['data'].data.shape    # >> (1, 49, 19, 19)
#print net.blobs['data'].diff.shape    # >> (1, 49, 19, 19)

#And for a convolutional layer:

#print net.blobs['conv1'].data.shape    # >> (1, 256, 19, 19)
#print net.blobs['conv2'].diff.shape    # >> (1, 256, 19, 19)

#print net.params['conv1_5x5_256'][0].data.shape  # >> (256, 49, 5, 5)
#print net.params['conv1_5x5_256'][0].data.shape[0] # >> 256
#print net.params['conv1_5x5_256'][0].data.shape[1] # >> 49
#print net.params['conv1_5x5_256'][0].data.shape[2] # >> 5
#print net.params['conv1_5x5_256'][0].data.shape[3] # >> 5


def short_str(s):
	r = '%.6g' % s
	#r = '%.3g' % s     # LZ style. this is maybe ok.
	u = r
	if ( r[0:2]== '0.' ) :
		u = r[1:]
	if ( r[0:3]=='-0.' ) :
		u = '-' + r[2:]
	return u
#    return '{0:.3f}'.format(s)
#    return '{0:.3f}'.format(s)

#bf = open('binary.bin', 'wb')
bf = open('binary.txt', 'w')
sum = 0
fc_sum = 0
cv_sum = 0

bf.write('2\n')    # version

n_layer = len( net.params.items() )
print n_layer


for loop in range(n_layer):
	name = net.params.items()[loop][0]
    #print loop , name
	a0 = net.params[name][0].data.shape[0]
	#print a0
	ct = 0;
	if 'bn' in name:
		a1 = net.params[name][1].data.shape[0]
		b0 = net.params[name][2].data.shape[0]
		print loop , name, a0,a1, ":", b0
		for i in range(2):
			ct = 0
			for j in range(a0):
				d = net.params[name][i].data[j]
				#bf.write(struct.pack("f", d))
				if ct==1: bf.write(' ')
				ct = 1
				bf.write(short_str(d))
				#bf.write(str(d))
				sum += 1
			bf.write('\n')
		# this is alwasys "999.982" ? -> needed! scale_factor
		d = net.params[name][2].data[0]
		#print "bn_scale_factor=", d
		#bf.write(struct.pack("f", d))
		#bf.write(str(d))
		#sum += 1
		#bf.write('\n')
		continue

	a1 = net.params[name][0].data.shape[1]
	if ('fc' in name or 'ip' in name):
		b0 = net.params[name][1].data.shape[0]
		print loop , name, a0,a1, ":", b0
		for i in range(a0):
			for j in range(a1):
				d = net.params[name][0].data[i][j]
				#bf.write(struct.pack("f", d))
				if ct==1: bf.write(' ')
				ct = 1
				#bf.write(str(d))
				bf.write(short_str(d))
				sum += 1
				fc_sum += 1
		bf.write('\n')
	else:
		a2 = net.params[name][0].data.shape[2]
		a3 = net.params[name][0].data.shape[3]
		b0 = net.params[name][1].data.shape[0]
		print loop , name, a0,a1,a2,a3, ":", b0

		for i in range(a0):
			for j in range(a1):
				for k in range(a2):
					for m in range(a3):
						d = net.params[name][0].data[i][j][k][m]
						#bf.write(struct.pack("f", d))
						if ct==1: bf.write(' ')
						ct = 1
						#bf.write(str(d))
						bf.write(short_str(d))
						sum += 1
						cv_sum += 1
		bf.write('\n')

	ct = 0
	for i in range(b0):
		d = net.params[name][1].data[i]
		#bf.write(struct.pack("f", d))
		if ct==1: bf.write(' ')
		ct = 1
		#bf.write(str(d))
		bf.write(short_str(d))
		sum += 1
	bf.write('\n')

bf.close()
print "convert done...", sum, " (fc_sum=", fc_sum, " cv_sum=", cv_sum, ")"
sys.exit()



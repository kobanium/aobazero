#rm -f caffe.INFO
./learn |& tee `date "+%Y%m%d_%H%M%S"`.log

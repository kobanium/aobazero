#!/usr/bin/env sh

#cp /tmp/caffe.INFO caffe.INFO
if [ $# != 1 ]; then
  echo "no filename"
  exit 1
fi

cp $1 caffe.INFO
~/caffe/tools/extra/parse_log.sh caffe.INFO

#gnuplot ~/caffe/tools/extra/plot_log.gnuplot_v.train
#gnuplot ~/caffe/tools/extra/plot_log.gnuplot.train

gnuplot ~/caffe/tools/extra/plot_loss_1.train
gnuplot ~/caffe/tools/extra/plot_loss_2.train
#gnuplot ~/caffe/tools/extra/plot_loss_3.train



import argparse
import os
import sys
from random import shuffle
import numpy as np

parser = argparse.ArgumentParser()

parser.add_argument(
	'--file',
	type=str,
	default='',
	help='file in libsvm format')

parser.add_argument(
	'--num_features',
	type=int,
	required=True,
	help='num_features')

args = parser.parse_args()

samples = []

f = open(args.file, 'r')

index = 0
for line in f:
	items = line.split()
	sample = np.zeros(args.num_features+1)
	sample[0] = float(items[0]) # label
	for j in range(1, len(items)):
		item = items[j].split(':')
		sample[int(item[0])] = float(item[1])
	samples.append(sample)
	print(index)
	index += 1

samples = np.asarray(samples)

print('samples.shape: ' + str(samples.shape) )

f_out = open(args.file + '_raw_' + str(samples.shape[0]) + '_' + str(samples.shape[1]), 'w');
samples.tofile(f_out)
f_out.close()
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import os
import sys
from random import shuffle

import numpy as np

def reformat_features(directory, file_numbers, label, num_samples):
	samples = []

	processed_samples = 0

	for i in file_numbers:
		filename = 'features' + str(i)
		f_in = open(directory + '/' + filename, 'r')

		for line in f_in:
			if 'synset_id' not in line and 'label' not in line:
				features = np.array(line.split(';')[2].split(' '))
				features = features[1:2049]
				sample = np.insert(features, 0, label)
				sample = sample.astype(np.float)

				samples.append(sample)

				# print(len(samples))
				# print(samples)
				if processed_samples == num_samples-1:
					break

				print(processed_samples)
				processed_samples += 1
		f_in.close()

	return samples

parser = argparse.ArgumentParser()

parser.add_argument(
	'--features_dir',
	type=str,
	default='',
	required=True,
	help='Absolute path to features directory.')
parser.add_argument(
	'--num_classes',
	type=int,
	default=8,
	required=True,
	help='num_classes')
parser.add_argument(
	'--num_samples',
	type=int,
	default=8,
	required=True,
	help='num_samples per class')
parser.add_argument(
	'--shuffle',
	required=True,
	type=int)

args = parser.parse_args()

samples = []

classes = [
281 	# 'tabby, tabby cat',
,282 	# 'tiger cat',
,283 	# 'Persian cat',
,284 	# 'Siamese cat, Siamese',
,285	# 'Egyptian cat',
,383	# 'Madagascar cat, ring-tailed lemur, Lemur catta',
,153	# 'Maltese dog, Maltese terrier, Maltese',
,235	# 'German shepherd, German shepherd dog, German police dog, alsatian',
,230	# 'Shetland sheepdog, Shetland sheep dog, Shetland',
,238	# 'Greater Swiss Mountain dog',
,200	# 'Tibetan terrier, chrysanthemum dog',
,230	# 'Shetland sheepdog, Shetland sheep dog, Shetland',
,1		#'goldfish, Carassius auratus',
,2		#'great white shark, white shark, man-eater, man-eating shark, Carcharodon carcharias',
,3		#'tiger shark, Galeocerdo cuvieri',
,4		#'hammerhead, hammerhead shark',
]
for c in range(0, args.num_classes):
	samples.extend( reformat_features(args.features_dir, [classes[c]], c, args.num_samples) )

if args.shuffle == 1:
	print('Shuffling...')
	shuffle(samples)

samples = np.asarray(samples)

print('samples.shape: ' + str(samples.shape) )

if args.shuffle == 1:
	file_name = args.features_dir + '/im_' + str(samples.shape[0]) + '_' + str(samples.shape[1]) + '_' + str(args.num_classes) + '.libsvm'
else:
	file_name = args.features_dir + '/im_' + str(samples.shape[0]) + '_' + str(samples.shape[1]) + '_' + str(args.num_classes) + '_nonshuffle.libsvm'

f_out = open(file_name, 'w');

for sample in samples:
	f_out.write(str(int(sample[0])) + " ")
	for j in range(0, 2048):
		f_out.write(str(j+1) + ":" + str(sample[j+1]) + " ")
	f_out.write("\n")
f_out.close()

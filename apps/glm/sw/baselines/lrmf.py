from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import os
import sys
from random import shuffle
import time

import numpy as np
import pandas as pd
from sklearn.decomposition import NMF
from scipy.sparse import coo_matrix

def RMSE(M, Utranspose, X):
	loss = 0.0
	for i in range(0,len(X.data)):
		dot = np.dot(M[X.row[i],:], Utranspose[X.col[i],:])
		error = dot - X.data[i]
		loss = loss + error*error
	loss = loss/len(X.data)
	return np.sqrt(loss)

parser = argparse.ArgumentParser()

parser.add_argument(
	'--train_file',
	type=str,
	required=1,
	help='Absolute path to the raw train file.')
parser.add_argument(
	'--num_features',
	type=int,
	required=1,
	help='num_features')
parser.add_argument(
	'--mdim',
	type=int,
	required=1,
	help='mdim')

args = parser.parse_args()

L = np.fromfile(args.train_file, dtype=np.uint32)

print("L.shape: " + str(L.shape))
print("Mdim: " + str(L[0]))

pos = 0
Mdim = L[pos]
pos += 1
Udim = 0

Mindex = []
Uindex = []
Value = []
count = 0
for i in range(0, Mdim):
	tempMindex = L[pos]
	pos += 1
	numEntries = L[pos]
	pos += 1

	for j in range(0, numEntries):
		count += 1
		Mindex.append( tempMindex )
		tempUindex = L[pos]
		Uindex.append( tempUindex )
		pos += 1
		Value.append ( L[pos] )
		pos += 1
		if (tempUindex+1 > Udim):
			Udim = tempUindex+1

	if (i == args.mdim-1):
		Mdim = args.mdim
		break

print("count: " + str(count))
print("Mdim: " + str(Mdim))
print("Udim: " + str(Udim))

row = np.array(Mindex)
col = np.array(Uindex)
data = np.array(Value)

X = coo_matrix((data, (row, col)), shape=(Mdim, Udim))
M = np.random.rand(Mdim, args.num_features)
U = np.random.rand(args.num_features, Udim)
print("M shape" + str(M.shape))
print("U shape" + str(U.shape))

loss = RMSE(M, np.transpose(U), X)
print("initial loss: " + str(loss))

model = NMF(n_components=args.num_features, init='custom', random_state=0, verbose=False, max_iter=10, solver='mu')

for e in range(0, 10):
	start = time.time()
	M = model.fit_transform(X, W=M, H=U)
	U = model.components_
	end = time.time()
	print("time per epoch: " + str(end-start))
	loss = RMSE(M, np.transpose(U), X)
	print("loss: " + str(loss))
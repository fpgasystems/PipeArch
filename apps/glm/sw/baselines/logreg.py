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
from sklearn.linear_model import LogisticRegression
from sklearn.linear_model import SGDClassifier
from sklearn.metrics import log_loss

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

args = parser.parse_args()

samples = np.fromfile(args.train_file, dtype=float)

print('len(samples): ' + str(len(samples)) )
num_samples = int(len(samples)/(args.num_features+1))
print('num_samples: ' + str(num_samples))

samples = np.reshape(samples, (num_samples, args.num_features+1))

X = samples[:,1:]
y = samples[:,0]

print('X.shape: ' + str(X.shape))
print('y.shape: ' + str(y.shape))

X_norm = X
mins = np.zeros(X.shape[1])
maxes = np.zeros(X.shape[1])
for j in range(0, X.shape[1]):
	mins[j] = np.min(X[:,j])
	maxes[j] = np.max(X[:,j])
	ranges = maxes[j] - mins[j]
	if ranges > 0.0:
		X_norm[:,j] = np.divide(X[:,j]-mins[j], ranges)

Bias = np.ones((X.shape[0],1))

X_norm = np.concatenate((Bias, X_norm), axis=1)

for i in range(0,2):
	print('X_norm[i]: ' + str(X_norm[i,:]))
	print('y[i]: ' + str(y[i]))

lmdba = 0.001

logreg = LogisticRegression(
	penalty='l1', 
	dual=False,
	tol=0.001,
	C=1.0/lmdba,
	fit_intercept=True,
	random_state=1,
	solver='liblinear',
	max_iter=10,
	multi_class='ovr',
	verbose=3,
	warm_start=True,
	n_jobs=1)

num_epochs = 1

total = 0.0
for epoch in range(0,num_epochs):
	start = time.time()
	logreg.fit(X_norm, y)
	end = time.time()
	total += (end-start)
	loss = log_loss(y, logreg.predict_proba(X_norm), labels=np.unique(y)) + lmdba*np.sum( np.abs(logreg.coef_) )
	print(np.abs(logreg.coef_))
	print(str(loss))

print('avg time per epoch: ' + str(total/num_epochs))

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
	'--tf',
	type=str,
	required=1,
	help='Absolute path to the raw train file.')
parser.add_argument(
	'--ns',
	type=int,
	required=1,
	help='num samples (only used if tf == syn)')
parser.add_argument(
	'--nf',
	type=int,
	required=1,
	help='num features')
parser.add_argument(
	'--nc',
	type=int,
	required=1,
	help='num classes')

args = parser.parse_args()

if (args.tf == 'syn'):
	samples = np.random.rand(args.ns, args.nf+1)
else:
	samples = np.fromfile(args.tf, dtype=float)

	print('len(samples): ' + str(len(samples)) )
	num_samples = int(len(samples)/(args.nf+1))
	print('num_samples: ' + str(num_samples))

	samples = np.reshape(samples, (num_samples, args.nf+1))

num_classes = int(args.nc)

X = samples[:,1:]
y = samples[:,0]

y = np.around(y)

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

lmbda = 0.0001
stepsize = 0.001

logreg = SGDClassifier(
	loss='log', 
	penalty='l2', 
	alpha=lmbda,
	fit_intercept=False, 
	max_iter=10,
	tol=0.01,
	shuffle=False, 
	verbose=0, 
	n_jobs=num_classes, 
	random_state=1, 
	learning_rate='optimal', 
	eta0=stepsize, 
	warm_start=True, 
	average=False)

num_epochs = 10

total = 0.0
for epoch in range(0,num_epochs):
	start = time.time()
	logreg.partial_fit(X_norm, y, classes=np.unique(y))
	end = time.time()
	total += (end-start)
	loss = log_loss(y, logreg.predict_proba(X_norm), labels=np.unique(y)) #+ lmbda*np.dot( logreg.coef_, logreg.coef_ )
	print(str(loss))

print('avg time per epoch: ' + str(total/num_epochs))

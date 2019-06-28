from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import os
import sys
from random import shuffle
import time
import threading

import numpy as np
import tensorflow as tf

# config = tf.ConfigProto(log_device_placement=True)
config = tf.ConfigProto()
config.gpu_options.per_process_gpu_memory_fraction = 0.8

def tf_logreg(num_features, num_classes, which_gpu):
	with tf.device('/device:GPU:'+str(which_gpu)):
		tf.set_random_seed(7)

		learning_rate = tf.placeholder("float", name='learning_rate')
		regularization = tf.placeholder("float", name='regularization')

		tf_x = tf.placeholder("float", [None, num_features], name='x')
		tf_y = tf.placeholder("int32", [None], name='y')

		x = tf.reshape(tf_x, [-1, num_features])
		y = tf.one_hot(tf_y, depth=num_classes, on_value=1.0, off_value=0.0)
		y = tf.reshape(y, [-1, num_classes])

		tf_w = tf.get_variable("model", [num_features, num_classes], dtype=tf.float32, initializer=tf.zeros_initializer)

		logits = tf.nn.sigmoid(tf.matmul(x, tf_w))

		# loss = tf.losses.softmax_cross_entropy(y, tf.log(logits))

		positiveLoss = tf.log(logits)
		negativeLoss = tf.log(1-logits)
		loss = tf.reduce_sum( tf.add(tf.multiply(y, positiveLoss), tf.multiply((1-y), negativeLoss)) )
		loss = -loss/(num_classes* tf.cast(tf.shape(y)[0], dtype=tf.float32) )

		loss = loss + regularization*tf.reduce_sum(tf.abs(tf_w))
		loss = tf.identity(loss, name="loss")

		optimizer = tf.train.GradientDescentOptimizer(learning_rate).minimize(loss)

		return tf_x, tf_y, tf_w, loss, optimizer, learning_rate, regularization

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
	'--num_classes',
	type=int,
	required=1,
	help='num_classes')
parser.add_argument(
	'--mb',
	type=int,
	required=1,
	help='minibatch size')
parser.add_argument(
	'--gpu',
	type=int,
	required=1,
	help='which gpu?')

args = parser.parse_args()

samples = np.fromfile(args.train_file, dtype=float)

print('len(samples): ' + str(len(samples)) )
print('len(samples)/(args.num_features+1): ' + str(len(samples)/(args.num_features+1)) )
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

num_epochs = 10
minibatch_size = args.mb
lr = 0.8
reg = 0.0001

num_features = X_norm.shape[1]
num_samples = y.shape[0]

def train_job():
	tf.reset_default_graph()
	tf_x, tf_y, tf_w, loss, optimizer, learning_rate, regularization = tf_logreg(X_norm.shape[1], args.num_classes, args.gpu)
	init = tf.global_variables_initializer()

	total = 0.0
	with tf.Session(config=config) as sess:
		sess.run(init)
		initial_loss = sess.run(loss, feed_dict={tf_x:X_norm, tf_y:y, learning_rate:0, regularization:reg})
		print('Inital loss: ' + str(initial_loss))

		for epoch in range(0, num_epochs):
			start = time.time()
			for i in range(0, int(y.shape[0]/minibatch_size) ):
				_ = sess.run(optimizer, feed_dict={
					tf_x:X_norm[i*minibatch_size:(i+1)*minibatch_size, :].reshape(minibatch_size,X_norm.shape[1]),
					tf_y:y[i*minibatch_size:(i+1)*minibatch_size].reshape(minibatch_size), 
					learning_rate:lr,
					regularization:reg})

			end = time.time()
			total += (end-start)

			epoch_loss = sess.run(loss, feed_dict={
				tf_x:X_norm,
				tf_y:y,
				learning_rate:0,
				regularization:reg})
			print(str(epoch_loss))

		print('avg time per epoch: ' + str(total/num_epochs))

for thread in range(0, 1): # to see what the GPU does with multiple threads running
	threading.Thread(target=train_job).start()

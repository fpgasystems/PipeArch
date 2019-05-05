from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import os
import sys
from random import shuffle
import time

import numpy as np
import tensorflow as tf

parser = argparse.ArgumentParser()

parser.add_argument(
	"--train_file",
	type=str,
	required=1,
	help="Absolute path to the raw train file.")
parser.add_argument(
	"--num_features",
	type=int,
	required=1,
	help="num_features")
parser.add_argument(
	"--mdim",
	type=int,
	required=1,
	help="mdim")

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

def tf_lrmf(Mdim, Udim, num_features):
	tf.set_random_seed(7)

	learning_rate = tf.placeholder("float", name="learning_rate")
	regularization = tf.placeholder("float", name="regularization")
	m_indexes = tf.placeholder("int32", [None], "m_indexes")
	u_indexes = tf.placeholder("int32", [None], "u_indexes")
	values = tf.placeholder("float", [None], "values")

	M = tf.Variable(initial_value=tf.truncated_normal([Mdim, num_features]), name="M")
	U = tf.Variable(initial_value=tf.truncated_normal([num_features, Udim]), name="U")

	result = tf.matmul(M, U)
	result_flatten = tf.reshape(result, [-1])
	R = tf.gather(result_flatten, m_indexes*tf.shape(result)[1] + u_indexes, name="extracting")

	diff_op = tf.subtract(R, values, name="trainig_diff")
	diff_op_squared = tf.abs(diff_op, name="squared_difference")
	base_cost = tf.reduce_sum(diff_op_squared, name="sum_squared_error")

	norm_sums = tf.add(
		tf.reduce_sum(tf.abs(M, name="M_abs"), name="M_norm"), 
		tf.reduce_sum(tf.abs(U, name="U_abs"), name="U_norm"))
	regularizer = tf.multiply(norm_sums, regularization, "regularizer")

	loss = tf.add(base_cost, regularizer, "loss")

	global_step = tf.Variable(0, trainable=False)
	optimizer = tf.train.GradientDescentOptimizer(learning_rate).minimize(loss, global_step=global_step)

	return learning_rate, regularization, m_indexes, u_indexes, values, loss, optimizer

lr = 0.0001
reg = 0
minibatch_size = len(data)

tf.reset_default_graph()
learning_rate, regularization, m_indexes, u_indexes, values, loss, optimizer = tf_lrmf(Mdim, Udim, args.num_features)
init = tf.global_variables_initializer()

with tf.Session() as sess:
	sess.run(init)
	initial_loss = sess.run(loss, feed_dict={m_indexes:row, u_indexes:col, values:data, learning_rate:0, regularization:reg})
	initial_loss = initial_loss/len(data)
	print('Inital loss: ' + str(initial_loss))

	for epoch in range(0, 10):
		start = time.time()
		for i in range(0, int(len(data)/minibatch_size) ):
			_ = sess.run(optimizer, feed_dict={
				m_indexes:row[i*minibatch_size:(i+1)*minibatch_size],
				u_indexes:col[i*minibatch_size:(i+1)*minibatch_size], 
				values:data[i*minibatch_size:(i+1)*minibatch_size],
				learning_rate:lr,
				regularization:reg})
		end = time.time()
		print('time per epoch: ' + str(end-start))

		epoch_loss = sess.run(loss, feed_dict={m_indexes:row, u_indexes:col, values:data, learning_rate:0, regularization:reg})
		epoch_loss = epoch_loss/len(data)
		print('epoch ' + str(epoch) + ': ' + str(epoch_loss))

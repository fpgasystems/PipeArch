import argparse
import os
import sys
from random import shuffle
import numpy as np

parser = argparse.ArgumentParser()

parser.add_argument(
	'--dir',
	type=str,
	default='',
	help='Absolute path to Netflix directory.')

parser.add_argument(
	'--file',
	type=str,
	default='',
	help='File name.')

args = parser.parse_args()

user_counter = 0
users = {}
movies = {}

f_in = open(args.dir + '/' + args.file, 'r')

movie_id = -1
for line in f_in:
	if ":" in line:
		movie_id = int(line.split(':')[0])-1
		print("movie_id: " + str(movie_id))
		# if movie_id == 2:
		# 	break

		movies[movie_id] = []
	else:
		items = line.split(',')
		user_id = int(items[0])
		rating = int(items[1])

		if user_id not in users:
			users[user_id] = user_counter
			user_counter = user_counter+1
		# else:
		# 	print("Duplicate user: " + str(user_id))

		movies[movie_id].append([users[user_id], rating])

for movie_id in movies:
	print("movie_id: " + str(movie_id))
	result = np.array(movies[movie_id]).flatten()
	result = np.insert(result, 0, len(movies[movie_id]))
	print("result.shape: " + str(result.shape))
	print(result)

	f_out = open(args.dir + '/rawfiles/movie' + str(movie_id) + '.raw', 'w');
	result.tofile(f_out)
	f_out.close()
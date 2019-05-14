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

		movies[movie_id].append([int(users[user_id]), int(rating)])

print("user_counter: " + str(user_counter))

f_out = open(args.dir + '/libmf/' + args.file.replace('txt', 'libmf'), 'w');

for movie in movies.items():
	for user_rating in movie[1]:
		f_out.write(str(movie[0]+1) + " " + str(user_rating[0]+1) + " " + str(user_rating[1]) + "\n")

f_out.close()
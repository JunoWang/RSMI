# !/usr/bin/python
# coding=utf-8

import sys, getopt
import os

import random
import numpy as np
import matplotlib.pyplot as plt
from sklearn import preprocessing
import matplotlib.patches as patches
from matplotlib.ticker import MultipleLocator
import configparser
import tensorflow as tf
import time

curPath = os.path.abspath(os.path.dirname(__file__))
rootPath = os.path.split(curPath)[0]
sys.path.append(rootPath)

os.environ["CUDA_DEVICE_ORDER"] = "PCI_BUS_ID"
os.environ["CUDA_VISIBLE_DEVICES"] = "-1"

def getUniformPoints(num, filename, dim):
    all_result = {}
    for i in range(dim - 1):
        all_result[i+2] = []
    for i in range(num):
        node_string = ''
        for j in range(dim):
            val = random.uniform(0, 1)
            node_string = node_string + str(val) + ","
            if j >= 1:
                all_result[j + 1].append(node_string + str(i) + "\n")
        # node_string = node_string + str(i) + "\n"
        # all_result.append(node_string)

    for j in range(dim - 1):
        name = filename % (num, j + 2)
        all_fo = open(name, "w")
        for i in range(num):
            all_fo.write(all_result[j+2][i])
        all_fo.close()

def getNormalPoints(num, filename, dim, mean=0.5, stddev=0.125):
    locations_tf = []
    for i in range(dim):
        locations_tf.append(tf.random.truncated_normal([num, 1], mean=0.5, stddev=0.125, dtype=tf.float32))
        # locations_tf.append(tf.random_normal([num * 2, 1], mean=mean, stddev=stddev, dtype=tf.float32))
    with tf.compat.v1.Session() as sees:
        locations = []
        for i in range(dim):
            locations.append(sees.run(locations_tf[i]))
        name = filename % (num, dim, mean, stddev)
        index = 0
        with open(name, "w") as fo:

            # for i in range(num * 2):
            while index < num and i < num:
                while True:
                    iswritable = True
                    node_string = ''
                    for j in range(dim):
                        if locations[j][i][0] < 0 or locations[j][i][0] > 1:
                            iswritable = False
                            break
                        node_string = node_string + str(locations[j][index][0]) + ","
                    if iswritable:
                        node_string = node_string + str(i) + "\n"
                        fo.write(node_string)
                        i += 1
                        index += 1
                        break
                    else:
                        i += 1

def getSkewedPoints(num, a, filename, dim):
    locations_tf = []
    for i in range(dim):
        locations_tf.append(tf.random.truncated_normal([num, 1], mean=0.5, stddev=0.25, dtype=tf.float32))
    with tf.compat.v1.Session() as sees:
        locations = []
        for i in range(dim):
            locations.append(sees.run(locations_tf[i]))
    # for a in range(1, 9, 2):
    name = filename % (num, a, dim)
    with open(name, "w") as fo:
        for i in range(num):
            node_string = ''
            for j in range(dim - 1):
                node_string = node_string + str(locations[j][i][0]) + ","
            node_string = node_string + str(locations[dim - 1][i][0] ** a) + "," + str(i) + "\n"
            fo.write(node_string)
    # all_result = {}
    # for i in range(dim - 1):
    #     all_result[i+2] = []
    # for i in range(num):
    #     node_string = ''
    #     for j in range(dim):
    #         val = random.uniform(0, 1)
    #         if j == dim - 1:
    #             val = val ** a
    #         node_string = node_string + str(val) + ","
    #         if j >= 1:
    #             all_result[j + 1].append(node_string + str(i) + "\n")
        # node_string = node_string + str(i) + "\n"
        # all_result.append(node_string)

    # for j in range(dim - 1):
    #     name = filename % (num, a, dim)
    #     all_fo = open(name, "w")
    #     for i in range(num):
    #         all_fo.write(all_result[j+2][i])
    #     all_fo.close()


def parser(argv):
    try:
        opts, args = getopt.getopt(argv, "d:s:n:f:m:")
    except getopt.GetoptError:
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-d':
            distribution = arg
        elif opt == '-s':
            size = int(arg)
        elif opt == '-n':
            skewness = int(arg)
        elif opt == '-f':
            filename = arg
        elif opt == '-m':
            dim = int(arg)
    return distribution, size, skewness, filename, dim

# python pre_train/data_gen.py -d uniform -s 10000 -n 1 -f /pre_train/data/uniform_10000_1_2_.csv -m 2
# python pre_train/data_gen.py -d normal -s 10000 -n 1 -f pre_train/data/normal_10000_1_2_.csv -m 2
# python pre_train/data_gen.py -d skewed -s 10000 -n 12 -f pre_train/data/skewed_10000_12_2_.csv -m 2

def gen(distribution, size, dim):
    if distribution == 'uniform':
        filename = "pre_train/data/uniform_%d_1_%d_.csv"
        getUniformPoints(size, filename, dim)
    elif distribution == 'normal':
        means = [0.0, 0.25, 0.5, 0.75, 1.0]
        stddevs = [0.125, 0.25, 0.5]
        filename = "pre_train/data/normal_%d_1_%d_%f_%f_.csv"
        for mean in means:
            for stddev in stddevs:
                getNormalPoints(size, filename, dim, mean, stddev)

    elif distribution == 'skewed':
        filename = "pre_train/data/skewed_%d_%d_%d_.csv"
        skewnesses = [2,3,4,5,6,7,8,9,10,11,12]
        for skewness in  skewnesses:
            getSkewedPoints(size, skewness, filename, dim)

if __name__ == '__main__':
    # distribution, size, skewness, filename, dim = parser(sys.argv[1:])
    distribution, size, dim = 'normal', 1000, 2
    gen(distribution, size, dim)
    distribution, size, dim = 'uniform', 1000, 2
    gen(distribution, size, dim)
    distribution, size, dim = 'skewed', 1000, 2
    gen(distribution, size, dim)
    
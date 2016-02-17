#!/usr/bin/python

import io
import os

def parse_file(dir, filep):
    tmpf = open(dir + filep)
    filep = filep[3:]
    filep = filep.replace(".", " ")
    if filep.find(" time") != -1:
        filep = filep[0:filep.find(" time")]
    else:
        filep = filep[0:filep.find(" out")]

    printstr=filep+","
    for line in tmpf:
        if "curhp" in line:
            intval = int(line[line.find(' '):-1])
            if intval != 0:
                printstr+=str(intval) + ","
    print printstr

def walk_files(dir):
    print dir
    dirlist = os.listdir(dir)
    dirlist.sort()
    for file in dirlist:
            parse_file(dir, file)

walk_files("./results_test_app/nonaggr/")
walk_files("./results_test_app/aggr/")

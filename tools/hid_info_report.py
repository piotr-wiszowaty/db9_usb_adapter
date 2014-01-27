#!/usr/bin/env python

# usage: python hid_info_report.py DEVICE-PATH REPORT-SIZE

import sys
import time

def show(data0, data):
    print "\033[2A%s" % " ".join(map(lambda c: "%02X" % ord(c), data0))
    print " ".join(map(lambda c: "%02X" % ord(c), data))

path = sys.argv[1]
report_size = int(sys.argv[2])

def get_info():
    f = open(path, "rb")
    data = f.read(report_size)
    f.close()
    return data

data0 = get_info()
print "\n"
while True:
    data = get_info()
    show(data0, data)
    time.sleep(0.25)

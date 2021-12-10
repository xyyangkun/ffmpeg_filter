#! /usr/bin/python3
import os
import time

if __name__ == "__main__":
    print("main")
    count=0
    while(1):
        os.system("date")
        os.system("time ./send_chang_resolution.py -a 192.168.2.235 -t 00")
        time.sleep(10)
        os.system("time ./send_chang_resolution.py -a 192.168.2.235 -t 07")
        time.sleep(10)
        os.system("time ./send_chang_resolution.py -a 192.168.2.235 -t 2a")
        time.sleep(10)
        count += 3
        print("=====================================time count=", count)


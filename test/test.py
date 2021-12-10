#!/usr/bin/python3
# -*- coding: UTF-8 -*-
import socket
import struct
import optparse
import binascii


import time
def prepare_sock(send_ip, port):
    #sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.connect((send_ip, port))
    return sock


# 导入 package_test 包
import package_test.send_v_screen_2_12 as send_v_screen
import package_test.send_v_screen_bind_6_12_test as send_v_screen_bind
import package_test.send_v_screen_unbind_7_12_test as send_v_screen_unbind
import package_test.send_v_signal_list_3 as send_v_signal
import package_test.send_v_signal_bind_4 as send_v_signal_bind
import package_test.send_v_signal_unbind_5 as send_v_signal_unbind
#import package_test.test import test
import package_test.test as test


if __name__ == '__main__':
    print("ok")
    parser = optparse.OptionParser()
    parser.add_option('-p', '--port', dest='port', default=8889, type='int', help='The port to send to')
    parser.add_option('-a', '--addr', dest='ip', default="172.20.2.4", type='string', help='The dest ip to send to')
    opts, args = parser.parse_args()
    sock = prepare_sock(opts.ip, opts.port)

    while 1:
        time.sleep(1)
        sock.send(send_v_screen_unbind.get_v_screen_data())
        sock.send(send_v_signal_unbind.get_v_signal_data())
        time.sleep(3)

        sock.send(send_v_screen.get_v_screen_data())
        sock.send(send_v_screen_bind.get_v_screen_data())

        sock.send(send_v_signal.get_v_signal_data())
        sock.send(send_v_signal_bind.get_v_signal_data())

        test.test()
        time.sleep(3)


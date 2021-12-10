#!/usr/bin/python3
# ---coding:utf-8-----
#--- tcp client ----

import socket
import struct
import optparse
import binascii
import crc16




HOST = '172.20.2.4'    # The remote host
PORT =  8889              # The same port as used by the server

def prepare_sock(send_ip, port):
    #sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.connect((send_ip, port))
    return sock

# 获取虚拟坐席布局
def get_v_big_screen_data(s_x, s_y, s_w, s_h, d_x, d_y, d_w, d_h):
    #_data = '\xab\xa6' # head
    _data = "eba6" # head
    _data += "02"  #group id
    _data += "0d"  #cmd id

    # data + crc32长度
    # data len= 1 + 5 + 9*4 + 9*2 +5 + 9*1 + 2
    # crc32 len= 2
    len = 4 + 8 + 8 + 2
    print("data len=", len)
    _data += binascii.b2a_hex(struct.pack("H", len)).decode()
    #_data += '{0:08x}'.format(socket.htons(len)) #长度
    hex_len = binascii.b2a_hex(struct.pack("H", len)).decode()
    print("hex_len = ", hex_len)
    print(str(hex_len))

    # ip len 4
    _data += '{0:02x}'.format(192) # TX IP
    _data += '{0:02x}'.format(168) # TX IP
    _data += '{0:02x}'.format(2) # TX IP
    _data += '{0:02x}'.format(237) # TX IP

    # 原视频要显示局部视频的区域
    _data += '{0:04x}'.format(socket.htons(s_x)) 
    _data += '{0:04x}'.format(socket.htons(s_y)) 
    _data += '{0:04x}'.format(socket.htons(s_w)) 
    _data += '{0:04x}'.format(socket.htons(s_h)) 

    # 显示视频在屏幕的位置 
    _data += '{0:04x}'.format(socket.htons(d_x))
    _data += '{0:04x}'.format(socket.htons(d_y))
    _data += '{0:04x}'.format(socket.htons(d_w))
    _data += '{0:04x}'.format(socket.htons(d_h))

    # crc32 长度2 
    #_data += "0000" #crc32 不计算
    _crc16 =  crc16.byte_crc16(binascii.a2b_hex(_data))
    _data += _crc16
    print("will send data:", _data);
    return binascii.a2b_hex(_data)
 

if __name__ == "__main__":
    print("ok")
    #parser = optparse.OptionParser()
    parser = optparse.OptionParser(add_help_option=False)
    parser.add_option('-p', '--port', dest='port', default=8889, type='int', help='The port to send to')
    parser.add_option('-a', '--addr', dest='ip', default="172.20.2.4", type='string', help='The dest ip to send to')
    # 默认的参数是将右下角的视频全屏显示
    parser.add_option('-x', '--sx', dest='s_x', default=960,  type='int', help='src video display info')
    parser.add_option('-y', '--sy', dest='s_y', default=540,  type='int', help='src video display info')
    parser.add_option('-w', '--sw', dest='s_w', default=960,  type='int', help='src video display info')
    parser.add_option('-h', '--sh', dest='s_h', default=540,  type='int', help='src video display info')
    parser.add_option('-X', '--dx', dest='d_x', default=00,   type='int', help='dst video display info')
    parser.add_option('-Y', '--dy', dest='d_y', default=00,   type='int', help='dst video display info')
    parser.add_option('-W', '--dw', dest='d_w', default=1920, type='int', help='dst video display info')
    parser.add_option('-H', '--dh', dest='d_h', default=1080, type='int', help='dst video display info')
    opts, args = parser.parse_args()
    tcp_sock = prepare_sock(opts.ip, opts.port)

    tcp_sock.send(get_v_big_screen_data(opts.s_x, opts.s_y, opts.s_w, opts.s_h, opts.d_x, opts.d_y, opts.d_w, opts.d_h))
    data = tcp_sock.recv(1024)
    print("return data:", data)

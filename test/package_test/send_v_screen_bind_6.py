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
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

def prepare_sock(send_ip, port):
    #sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.connect((send_ip, port))
    return sock

# 获取虚拟坐席布局
def get_v_signal_data():
    #_data = '\xab\xa6' # head
    _data = "eba6" # head
    _data += "02"  #group id
    _data += "06"  #cmd id      

    # data + crc32长度
    # data len= 
    # crc32 len= 2
    len = 2 + 6 * 5 +2
    print("data len=", len)
    _data += binascii.b2a_hex(struct.pack("H", len)).decode()
    #_data += '{0:08x}'.format(socket.htons(len)) #长度
    hex_len = binascii.b2a_hex(struct.pack("H", len)).decode()
    print("hex_len = ", hex_len)
    print(str(hex_len))


    # 信号源列表集合
    _data += "05"  # 绑定信号源数量
    _data += "06"  # 单个信号源字长
    #--- 2

    ##  信号源0
    _data += '{0:02x}'.format(0) # 屏幕id
    _data += '{0:02x}'.format(0) # 划分屏id
    _data += '{0:02x}'.format(192) # TX IP
    _data += '{0:02x}'.format(168) # TX IP
    _data += '{0:02x}'.format(2) # TX IP
    _data += '{0:02x}'.format(237) # TX IP
    #--- 6
    ##  信号源1
    _data += '{0:02x}'.format(0) # 屏幕id
    _data += '{0:02x}'.format(1) # 划分屏id
    _data += '{0:02x}'.format(192) # TX IP
    _data += '{0:02x}'.format(168) # TX IP
    _data += '{0:02x}'.format(2) # TX IP
    _data += '{0:02x}'.format(237) # TX IP
    #--- 6
    ##  信号源2
    _data += '{0:02x}'.format(0) # 屏幕id
    _data += '{0:02x}'.format(2) # 划分屏id
    _data += '{0:02x}'.format(192) # TX IP
    _data += '{0:02x}'.format(168) # TX IP
    _data += '{0:02x}'.format(2) # TX IP
    _data += '{0:02x}'.format(237) # TX IP
    #--- 6
    ##  信号源3
    _data += '{0:02x}'.format(0) # 屏幕id
    _data += '{0:02x}'.format(3) # 划分屏id
    _data += '{0:02x}'.format(192) # TX IP
    _data += '{0:02x}'.format(168) # TX IP
    _data += '{0:02x}'.format(2) # TX IP
    _data += '{0:02x}'.format(237) # TX IP
    #--- 6
    ##  信号源4
    _data += '{0:02x}'.format(1) # 屏幕id
    _data += '{0:02x}'.format(0) # 划分屏id
    _data += '{0:02x}'.format(192) # TX IP
    _data += '{0:02x}'.format(168) # TX IP
    _data += '{0:02x}'.format(2) # TX IP
    _data += '{0:02x}'.format(237) # TX IP
    #--- 6
 
 
 
    



    # crc32 长度2 
    #_data += "0000" #crc32 不计算
    _crc16 =  crc16.byte_crc16(binascii.a2b_hex(_data))
    _data += _crc16
    return binascii.a2b_hex(_data)
 

if __name__ == "__main__":
    print("ok")
    parser = optparse.OptionParser()
    parser.add_option('-p', '--port', dest='port', default=8889, type='int', help='The port to send to')
    parser.add_option('-a', '--addr', dest='ip', default="172.20.2.4", type='string', help='The dest ip to send to')
    opts, args = parser.parse_args()
    tcp_sock = prepare_sock(opts.ip, opts.port)

    tcp_sock.send(get_v_signal_data())
    data = tcp_sock.recv(1024)

#!/usr/bin/python3
# ---coding:utf-8-----
#--- tcp client ----

import socket
import struct
import optparse
import binascii




HOST = '172.20.2.4'    # The remote host
PORT =  8889              # The same port as used by the server

def prepare_sock(send_ip, port):
    #sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.connect((send_ip, port))
    return sock

# 获取虚拟坐席布局
def get_v_screen_data():
    #_data = '\xab\xa6' # head
    _data = "eba6" # head
    _data += "02"  #group id
    _data += "02"  #cmd id      

    # data + crc32长度
    # data len= 1 + 5 + 9*4 + 9*2 +5 + 9*1 + 2
    # crc32 len= 2
    len = 1 + (5+9*4)*3 + 2 
    print("data len=", len)
    _data += binascii.b2a_hex(struct.pack("H", len)).decode()
    #_data += '{0:08x}'.format(socket.htons(len)) #长度
    hex_len = binascii.b2a_hex(struct.pack("H", len)).decode()
    print("hex_len = ", hex_len)
    print(str(hex_len))

    # 屏幕数量
    _data += "03"
    #----- 1

    # 屏幕列表0
    _data += "00"  #屏幕id
    _data += "02"  # 话分行
    _data += "02"  # 划分列
    _data += "04"  # 划分屏数量
    _data += "09"  # 划分屏列表单条数据长度
    #--- 5

    x=600
    y=400
    w=200
    h= 200

    ##  屏幕列表0 划分屏0
    _data += "00" # 划分屏id
    _data += '{0:04x}'.format(socket.htons(x)) # 划分屏x
    _data += '{0:04x}'.format(socket.htons(y)) # 划分屏y
    _data += '{0:04x}'.format(socket.htons(w)) # 划分屏w
    _data += '{0:04x}'.format(socket.htons(h)) # 划分屏h
    #--- 9
    ##  屏幕列表0 划分屏1
    _data += "01" # 划分屏id
    _data += '{0:04x}'.format(socket.htons(x+w)) # 划分屏x
    _data += '{0:04x}'.format(socket.htons(y)) # 划分屏y
    _data += '{0:04x}'.format(socket.htons(w)) # 划分屏w
    _data += '{0:04x}'.format(socket.htons(h)) # 划分屏h
    #--- 9
    ##  屏幕列表0 划分屏2
    _data += "02" # 划分屏id
    _data += '{0:04x}'.format(socket.htons(x)) # 划分屏x
    _data += '{0:04x}'.format(socket.htons(y+h)) # 划分屏y
    _data += '{0:04x}'.format(socket.htons(w)) # 划分屏w
    _data += '{0:04x}'.format(socket.htons(h)) # 划分屏h
    #--- 9
    ##  屏幕列表0 划分屏3
    _data += "03" # 划分屏id
    _data += '{0:04x}'.format(socket.htons(x+w)) # 划分屏x
    _data += '{0:04x}'.format(socket.htons(y+h)) # 划分屏y
    _data += '{0:04x}'.format(socket.htons(w)) # 划分屏w
    _data += '{0:04x}'.format(socket.htons(h)) # 划分屏h
    #--- 9

    # 屏幕列表1
    _data += "01"  #屏幕id
    _data += "02"  # 话分行
    _data += "02"  # 划分列
    _data += "04"  # 划分屏数量
    _data += "09"  # 划分屏列表单条数据长度
    #--- 5

    x=600+400; y=400
    w=200; h= 200

    ##  屏幕列表1 划分屏0
    _data += "00" # 划分屏id
    _data += '{0:04x}'.format(socket.htons(x)) # 划分屏x
    _data += '{0:04x}'.format(socket.htons(y)) # 划分屏y
    _data += '{0:04x}'.format(socket.htons(w)) # 划分屏w
    _data += '{0:04x}'.format(socket.htons(h)) # 划分屏h
    #--- 9
    ##  屏幕列表1 划分屏1
    _data += "01" # 划分屏id
    _data += '{0:04x}'.format(socket.htons(x+w)) # 划分屏x
    _data += '{0:04x}'.format(socket.htons(y)) # 划分屏y
    _data += '{0:04x}'.format(socket.htons(w)) # 划分屏w
    _data += '{0:04x}'.format(socket.htons(h)) # 划分屏h
    #--- 9
    ##  屏幕列表1 划分屏2
    _data += "02" # 划分屏id
    _data += '{0:04x}'.format(socket.htons(x)) # 划分屏x
    _data += '{0:04x}'.format(socket.htons(y+h)) # 划分屏y
    _data += '{0:04x}'.format(socket.htons(w)) # 划分屏w
    _data += '{0:04x}'.format(socket.htons(h)) # 划分屏h
    #--- 9
    ##  屏幕列表1 划分屏3
    _data += "03" # 划分屏id
    _data += '{0:04x}'.format(socket.htons(x+w)) # 划分屏x
    _data += '{0:04x}'.format(socket.htons(y+h)) # 划分屏y
    _data += '{0:04x}'.format(socket.htons(w)) # 划分屏w
    _data += '{0:04x}'.format(socket.htons(h)) # 划分屏h
    #--- 9

    # 屏幕列表2
    _data += "02"  #屏幕id
    _data += "02"  # 话分行
    _data += "02"  # 划分列
    _data += "04"  # 划分屏数量
    _data += "09"  # 划分屏列表单条数据长度
    #--- 5

    x=600+400+400; y=400
    w=200; h= 200

    ##  屏幕列表2 划分屏0
    _data += "00" # 划分屏id
    _data += '{0:04x}'.format(socket.htons(x)) # 划分屏x
    _data += '{0:04x}'.format(socket.htons(y)) # 划分屏y
    _data += '{0:04x}'.format(socket.htons(w)) # 划分屏w
    _data += '{0:04x}'.format(socket.htons(h)) # 划分屏h
    #--- 9
    ##  屏幕列表2 划分屏1
    _data += "01" # 划分屏id
    _data += '{0:04x}'.format(socket.htons(x+w)) # 划分屏x
    _data += '{0:04x}'.format(socket.htons(y)) # 划分屏y
    _data += '{0:04x}'.format(socket.htons(w)) # 划分屏w
    _data += '{0:04x}'.format(socket.htons(h)) # 划分屏h
    #--- 9
    ##  屏幕列表2 划分屏2
    _data += "02" # 划分屏id
    _data += '{0:04x}'.format(socket.htons(x)) # 划分屏x
    _data += '{0:04x}'.format(socket.htons(y+h)) # 划分屏y
    _data += '{0:04x}'.format(socket.htons(w)) # 划分屏w
    _data += '{0:04x}'.format(socket.htons(h)) # 划分屏h
    #--- 9
    ##  屏幕列表2 划分屏3
    _data += "03" # 划分屏id
    _data += '{0:04x}'.format(socket.htons(x+w)) # 划分屏x
    _data += '{0:04x}'.format(socket.htons(y+h)) # 划分屏y
    _data += '{0:04x}'.format(socket.htons(w)) # 划分屏w
    _data += '{0:04x}'.format(socket.htons(h)) # 划分屏h
    #--- 9

    

    # crc32 长度2 
    _data += "0000" #crc32 不计算
    return binascii.a2b_hex(_data)
 

if __name__ == "__main__":
    print("ok")
    parser = optparse.OptionParser()
    parser.add_option('-p', '--port', dest='port', default=8889, type='int', help='The port to send to')
    parser.add_option('-a', '--addr', dest='ip', default="172.20.2.4", type='string', help='The dest ip to send to')
    opts, args = parser.parse_args()
    tcp_sock = prepare_sock(opts.ip, opts.port)

    tcp_sock.send(get_v_screen_data())
    data = tcp_sock.recv(1024)

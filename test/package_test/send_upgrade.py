#!/usr/bin/python3
# ---coding:utf-8-----
#--- udp send upgrade file ---
# 

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

# 准备数据 
# type = 02 服务的准备数据 01 写数据且有后续数据 00 最后一包数据
def get_upgrade_data(msg_type, upgrade_data=""):
    #_data = '\xab\xa6' # head
    _data = "eba6" # head
    _data += "02"  #group id
    _data += "20"  #cmd id      

    # data + crc32长度
    data_len = 4 + 2 + len(upgrade_data)
    print("data len=", data_len)
    _data += binascii.b2a_hex(struct.pack("H", data_len)).decode()
    #_data += '{0:08x}'.format(socket.htons(len)) #长度
    hex_len = binascii.b2a_hex(struct.pack("H", data_len)).decode()
    print("hex_len = ", hex_len)
    print(str(hex_len))
   
    # s_upgrade
    # 组准备包
    print("type update_data:", type(upgrade_data), " data_len=", len(upgrade_data))
    upgrade_len = len(upgrade_data)
    _data += binascii.b2a_hex(struct.pack("H", upgrade_len)).decode()
    # 类型
    _data += msg_type
    # 未使用数据
    _data += "00"
    if upgrade_len != 0:
        #_data += binascii.a2b_hex(upgrade_data)
        _data += upgrade_data.hex()
   
    # crc32 长度2 
    #_data += "0000" #crc32 不计算
    _crc16 =  crc16.byte_crc16(binascii.a2b_hex(_data))
    _data += _crc16
    #print("data: " , _data[:-4])
    print("crc16 value:", _crc16)
    #print("all data:", _data)

    #print("data: " , _data[:-4])
    #print("datacrc:",  crc16.byte_crc16(binascii.a2b_hex(_data[:-4])))
    send_data =  binascii.a2b_hex(_data)
    print("send_data len:%d\n"%(len(send_data)))
    return  send_data
 

if __name__ == "__main__":
    print("ok")
    parser = optparse.OptionParser()
    parser.add_option('-p', '--port', dest='port', default=8889, type='int', help='The port to send to')
    parser.add_option('-a', '--addr', dest='ip', default="172.20.2.4", type='string', help='The dest ip to send to')
    parser.add_option('-u', '--updatefile', dest='updatefile', default="tx_upgrade_file_v0.1.bin", type='string', help='update file path and name')
    opts, args = parser.parse_args()
    tcp_sock = prepare_sock(opts.ip, opts.port)

    read_size = 60000
    print("update file:", opts.updatefile)
    # 0. 打开文件
    fo = open(opts.updatefile, "rb")

    # 1. 发送准备数据
    tcp_sock.send(get_upgrade_data("02"))
    data = tcp_sock.recv(1024)
    print("recv send prepare data:", data)
    
    count = 0;
    # 2. 循环读取文件发送给server
    while True:
        read_data = ''
        read_data = fo.read(read_size)
        print("read count %d , read size:%d"%(count, len(read_data)))
        if len(read_data) == read_size:
            tcp_sock.send(get_upgrade_data("01", read_data))
            data = tcp_sock.recv(1024)
            #print("recv send prepare data:", data)
        else:
            # 最后一包数据读完
            tcp_sock.send(get_upgrade_data("00", read_data))
            data = tcp_sock.recv(1024)
            print("recv send prepare data:", data)
            break
        count += 1


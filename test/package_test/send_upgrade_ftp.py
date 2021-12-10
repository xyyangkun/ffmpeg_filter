#! /usr/bin/python3
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

def get_upgrade_ftp_bak(user, passwd, path):
    #_data = '\xab\xa6' # head
    _data = "eba6" # head
    _data += "02"  #group id
    _data += "30"  #cmd id

    # path len
    _path_len = len(path)
    if 0 == _path_len:
        print("ERROR in path:", path)
        return ""

    # data + crc32长度
    _len = 4 + 64 + 64 + 2 + _path_len  + 2
    print("data len=", _len)
    _data += binascii.b2a_hex(struct.pack("H", _len)).decode()
    #_data += '{0:08x}'.format(socket.htons(len)) #长度
    hex_len = binascii.b2a_hex(struct.pack("H", _len)).decode()
    print("hex_len = ", hex_len)
    print(str(hex_len))

    # ftp server ip
    _data += '{0:02x}'.format(192)
    _data += '{0:02x}'.format(168) 
    _data += '{0:02x}'.format(2) 
    _data += '{0:02x}'.format(227) 


    print("before data:", _data)
    print("before data type:", type(_data))
    
    # user, passwd
    _user = bytearray(64)
    _user[:len(user)] = user
    _passwd = bytearray(64)
    _passwd[:len(passwd)] = passwd

    hex_user   = binascii.b2a_hex(_user).decode()
    hex_passwd = binascii.b2a_hex(_passwd).decode()

    print("hex_user:", hex_user)
    print("hex_passwd:", hex_passwd)

    print("\n\n\n")

    _data += hex_user
    print("hex_user+:", _data)
    _data += hex_passwd
    print("hex_passwd:", _data)

    
    hex_path_len = binascii.b2a_hex(struct.pack("H", _path_len)).decode()
    _data += hex_path_len

    # path
    hex_path = binascii.b2a_hex(path).decode()
    _data += hex_path

    print("path len:", _path_len)
    print("hex path len:", hex_path_len)
    print("hex_path : ", hex_path)
    print("hex_path +:  ", _data)

    # crc32 长度2 
    #_data += "0000" #crc32 不计算
    _crc16 =  crc16.byte_crc16(binascii.a2b_hex(_data))
    _data += _crc16
    return binascii.a2b_hex(_data)
 
def get_upgrade_ftp_ret():
    #_data = '\xab\xa6' # head
    _data = "eba6" # head
    _data += "02"  #group id
    _data += "32"  #cmd id
    error_code = 1;
    _data += binascii.b2a_hex(struct.pack("H", error_code)).decode()

    # 长度
    _len = 6 + 2
    _data += binascii.b2a_hex(struct.pack("H", _len)).decode()

    _crc16 =  crc16.byte_crc16(binascii.a2b_hex(_data))
    _data += _crc16
    return binascii.a2b_hex(_data)



def get_upgrade_ftp(user, passwd, path):
    #_data = '\xab\xa6' # head
    _data = "eba6" # head
    _data += "02"  #group id
    _data += "31"  #cmd id

    # path len
    _path_len = len(path)
    if 0 == _path_len:
        print("ERROR in path:", path)
        return ""

    _user_len = len(user)
    _pass_len = len(passwd)

    # data + crc32长度
    _len = 4 + 1 + _user_len + 1 + _pass_len +  2 +  _path_len  + 2
    print("data len=", _len)
    _data += binascii.b2a_hex(struct.pack("H", _len)).decode()
    #_data += '{0:08x}'.format(socket.htons(len)) #长度
    hex_len = binascii.b2a_hex(struct.pack("H", _len)).decode()
    print("hex_len = ", hex_len)
    print(str(hex_len))

    # ftp server ip
    _data += '{0:02x}'.format(192)
    _data += '{0:02x}'.format(168) 
    _data += '{0:02x}'.format(2) 
    _data += '{0:02x}'.format(105) 


    print("before data:", _data)
    print("before data type:", type(_data))
    

    hex_user   = binascii.b2a_hex(user).decode()
    hex_passwd = binascii.b2a_hex(passwd).decode()

    print("hex_user:", hex_user)
    print("hex_passwd:", hex_passwd)

    print("\n\n\n")

    #_data = binascii.b2a_hex(struct.pack("c", _user_len)).decode()
    _data += '{0:02x}'.format(_user_len)
    _data += hex_user
    print("hex_user+:", _data)
    #_data = binascii.b2a_hex(struct.pack("c", _pass_len)).decode()
    _data += '{0:02x}'.format(_pass_len)
    _data += hex_passwd
    print("hex_passwd:", _data)

    
    hex_path_len = binascii.b2a_hex(struct.pack("H", _path_len)).decode()
    _data += hex_path_len

    # path
    hex_path = binascii.b2a_hex(path).decode()
    _data += hex_path

    print("path len:", _path_len)
    print("hex path len:", hex_path_len)
    print("hex_path : ", hex_path)
    print("hex_path +:  ", _data)

    # crc32 长度2 
    #_data += "0000" #crc32 不计算
    _crc16 =  crc16.byte_crc16(binascii.a2b_hex(_data))
    _data += _crc16
    return binascii.a2b_hex(_data)
 

if __name__ == "__main__":
    print("ok")
    parser = optparse.OptionParser()
    parser.add_option('-p', '--port', dest='port', default=8882, type='int', help='The port to send to')
    parser.add_option('-a', '--addr', dest='ip', default="172.20.2.4", type='string', help='The dest ip to send to')
    # 海思busybox 的默认用户名是root, 密码是空:"" 
    parser.add_option('-u', '--user', dest='user', default="anonymous", type='string', help='ftp user')
    parser.add_option('-P', '--pass', dest='passwd', default="User@", type='string', help='ftp passwd')
    parser.add_option('-r', '--route', dest='path', default="/upgrade.bin", type='string', help='ftp get path')
    opts, args = parser.parse_args()
    tcp_sock = prepare_sock(opts.ip, opts.port)

    len = tcp_sock.send(get_upgrade_ftp(opts.user.encode(), opts.passwd.encode(), opts.path.encode()))
    print("send len:", len)
    data = tcp_sock.recv(1024)
    print("recv data        :", data.hex())

    # 升级接收另外一包数据
    data = tcp_sock.recv(1024)
    print("recv another data:", data.hex())
    # 返回设局
    tcp_sock.send(get_upgrade_ftp_ret())

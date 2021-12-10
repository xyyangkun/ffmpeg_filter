#!/usr/bin/python3
# ---coding:utf-8-----
#--- tcp client ----
# crc16 modbus
#https://www.cnblogs.com/oukunqing/p/5822781.html
# http://www.ip33.com/crc.html
def crc16(x, invert=True):
    a = 0xFFFF
    b = 0xA001
    for byte in x:
        a ^= ord(byte)
        for i in range(8):
            last = a % 2
            a >>= 1
            if last == 1:
                a ^= b
    s = hex(a).upper()
    
    return s[4:6]+s[2:4] if invert == True else s[2:4]+s[4:6]

def byte_crc16(x, invert=True):
    a = 0xFFFF
    b = 0xA001
    for byte in x:
        a ^= byte
        for i in range(8):
            last = a % 2
            a >>= 1
            if last == 1:
                a ^= b
    crc16_value = format(a, "04X")
    #print("crc16 value=%s"%(crc16_value))
    return crc16_value[2:4] + crc16_value[0:2] if invert == True else crc16_value

    #s = hex(a).upper()
    #return s[4:6]+s[2:4] if invert == True else s[2:4]+s[4:6]


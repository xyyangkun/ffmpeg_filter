#!/usr/bin/python3
# -*- coding: UTF-8 -*-
import binascii
import crc16
def test():
    print("test")
    data="eba6020008003412ac140203"
    #crc = crc16.crc16(data.decode('hex'))
    hex_data = bytes.fromhex(data)
    print("hex_data:", type(hex_data))
    crc = crc16.byte_crc16(hex_data)
    print(crc)

    crc = crc16.crc16(data)
    print(crc)
    print(crc16.crc16("012345678"))
    print(crc16.crc16("012345678", False))
    print(type("ab"))

    print("ab")
    # 把 字符串"ab" 转换成16进制数据
    #hex_data = "ab".decode('hex')
    hex_data = bytes.fromhex("ab")
    print("hex_data type:", type(hex_data), "hex_data conten:", hex_data)

    # 把hex转换成字符串
    str_data = binascii.b2a_hex(hex_data)
    print("str_data type:", type(str_data), "str_data", str_data)

if __name__ == '__main__':
    test()

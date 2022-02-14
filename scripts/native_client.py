#!/usr/bin/env python3
'''A native client simulating the plugin to use for testing the server'''
import typing
import os
import ctypes
import subprocess
import time
import json
from pathlib import Path

def encode_msg(message: str) -> bytearray:
    myjson = json.dumps({"text": message, "die": False})
    msglen = len(myjson.encode('utf-8'))
    len_in_bytes = bytes(ctypes.c_int(msglen))
    msg_in_bytes = bytes(myjson.encode('utf-8'))
    return len_in_bytes + msg_in_bytes

def get_translateLocally() -> subprocess.Popen:
    a = open('/tmp/testa', 'w')
    return subprocess.Popen([str(Path(__file__).resolve().parent) + "/../build/translateLocally", "-p"], stdout=a, stderr=subprocess.STDOUT, stdin=subprocess.PIPE) # subprocess.PIPE

if __name__ == '__main__':
    p = get_translateLocally()
    msg = encode_msg("Hello world!")
    msg2 = encode_msg("Sticks and stones may break my bones but words WILL NEVER HURT ME!")
    msg3 = encode_msg("Why does it not work well...")
    mgsgs = msg + msg2 + msg3
    try:
        print(p.communicate(input=mgsgs, timeout=1))
    except subprocess.TimeoutExpired:
        pass
    print(mgsgs)
    time.sleep(4)
    p.terminate()
    print(p.stdout.read().decode('utf-8',errors='ignore'))

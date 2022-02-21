#!/usr/bin/env python3
'''A native client simulating the plugin to use for testing the server'''
import typing
import os
import ctypes
import subprocess
import time
import json
from pathlib import Path

def encode_msg(message: str, src: str, trg: str) -> bytearray:
    global counter
    myjson = json.dumps({"command": "Translate", "id": counter, "data": {"text": message, "src": src, "trg": trg, "html": False}})
    msglen = len(myjson.encode('utf-8'))
    len_in_bytes = bytes(ctypes.c_int(msglen))
    msg_in_bytes = bytes(myjson.encode('utf-8'))
    counter = counter + 1
    return len_in_bytes + msg_in_bytes

def encode_list_msg(fetchremote: bool=True) -> bytearray:
    global counter
    myjson = json.dumps({"command": "ListModels", "id": counter, "data": {"includeRemote": fetchremote}})
    msglen = len(myjson.encode('utf-8'))
    len_in_bytes = bytes(ctypes.c_int(msglen))
    msg_in_bytes = bytes(myjson.encode('utf-8'))
    counter = counter + 1
    return len_in_bytes + msg_in_bytes

def encode_dwn_msg(modelname: str) -> bytearray:
    global counter
    myjson = json.dumps({"command": "DownloadModel", "id": counter, "data": {"modelID": modelname}})
    msglen = len(myjson.encode('utf-8'))
    len_in_bytes = bytes(ctypes.c_int(msglen))
    msg_in_bytes = bytes(myjson.encode('utf-8'))
    counter = counter + 1
    return len_in_bytes + msg_in_bytes

def get_translateLocally() -> subprocess.Popen:
    a = open('/tmp/testa', 'w')
    return subprocess.Popen([str(Path(__file__).resolve().parent) + "/../build/translateLocally", "-p"], stdout=a, stderr=subprocess.STDOUT, stdin=subprocess.PIPE) # subprocess.PIPE

if __name__ == '__main__':
    counter = 0
    p = get_translateLocally()
    msg = encode_msg("Hello world!", "en", "de")
    msg2 = encode_msg("Sticks and stones may break my bones but words WILL NEVER HURT ME!", "en", "es")
    msg3 = encode_msg("¿Por qué no funciona bien?", "es", "de")
    msg4 = encode_list_msg();
    msg5 = encode_dwn_msg("en-cs-tiny")
    mgsgs = msg + msg2 + msg3 + msg4 + msg5
    try:
        print(p.communicate(input=mgsgs, timeout=1))
    except subprocess.TimeoutExpired:
        pass
    print("Waiting for translateLocally to finish...")
    p.wait()
    print(mgsgs)
    with open('/tmp/testa', 'r', encoding='utf-8', errors='ignore') as a:
        for line in a:
            print(line.strip())

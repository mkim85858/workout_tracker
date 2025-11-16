# pose_inference.py
import socket, time

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(("127.0.0.1", 5005))  # or "127.0.0.1" if on same machine

while True:
    s.send(b'0')
    time.sleep(2)
    s.send(b'1')
    time.sleep(2)
    s.send(b'2')
    time.sleep(2)
    s.send(b'3')
    time.sleep(2)
    s.send(b'4')
    time.sleep(2)
    s.send(b'2')
    time.sleep(2)
import socket, random, time, sys
random.seed(7)
def send(payload, port=18080, wait=0.05):
    try:
        s = socket.create_connection(("127.0.0.1", port), timeout=2)
        s.sendall(payload); time.sleep(wait)
        try: s.recv(4096)
        except Exception: pass
        s.close()
    except Exception as e:
        return str(e)
cases = [
 b"GET / HTTP/1.1\r\n\r\n", b"\r\n\r\n", b"", b"\x00"*4096,
 b"GET /" + b"A"*70000 + b" HTTP/1.1\r\nHost: x\r\n\r\n",
 b"GET / HTTP/1.1\r\nHost: " + b"h"*5000 + b"\r\n\r\n",
 b"GET / HTTP/1.1\r\nOrigin: " + b"o"*5000 + b"\r\nHost: a\r\n\r\n",
 b"GET / HTTP/1.1\r\nOrigin: null\r\n\r\n",
 b"POST / HTTP/1.1\r\nHost: a\r\nContent-Length: -1\r\n\r\nabc",
 b"POST / HTTP/1.1\r\nHost: a\r\nContent-Length: 99999999999\r\n\r\nabc",
 b"POST / HTTP/1.1\r\nHost: a\r\nContent-Length: abc\r\n\r\n",
 b"OPTIONS * HTTP/1.1\r\nHost: a\r\n\r\n", b"GET /log=99999999999 HTTP/1.1\r\n\r\n",
 b"GET /servstats HTTP/1.1\r\nHost: a\r\n\r\n", b"GET /servstats.txt HTTP/1.1\r\n\r\n",
 b"GET /ca.crt HTTP/1.1\r\nHost: a\r\n\r\n", b"GET / \r\n", b"G", b" ",
]
for i in range(300):
    cases.append(bytes(random.getrandbits(8) for _ in range(random.randint(1, 3000))))
    cases.append(b"GET /" + bytes(random.choice(b"abc%/.?=&\r\n :") for _ in range(random.randint(1,500))) + b" HTTP/1.1\r\nHost: t\r\n\r\n")
for c in cases: send(c)
print("sent", len(cases), "cases")

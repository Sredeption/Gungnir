#!/usr/bin/env python
from remote import Server
from remote import KillServer
from remote import Client
import time

hosts = []
numbers = [4, 20, 2, 3, 5, 6, 14, 15, 16, 18, 19, 21]
# numbers = [4, 20]

for number in numbers:
    hosts.append("10.22.1.%d" % number)

server_host = hosts[0]
client_hosts = hosts[1:]

client_binary = "benchmark --time 3 --targetOps 20000"

if __name__ == '__main__':
    server = Server(server_host)
    server.start()

    time.sleep(2)

    clients = []
    for client_host in client_hosts:
        client = Client(client_host, client_binary, server_host)
        client.start()
        clients.append(client)

    for client in clients:
        client.wait()

    kill_server = KillServer(server_host)
    kill_server.start()
    kill_server.wait()
    server.wait()

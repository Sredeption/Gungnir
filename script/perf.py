#!/usr/bin/env python
import subprocess
import os
import time

hosts = []
numbers = [3, 4, 5, 14, 18, 19]

for number in numbers:
    hosts.append("10.22.1.%d" % number)

server_host = hosts[0]
client_hosts = hosts[1:]

server_binary = "gungnir"
client_binary = "benchmark"
port = 3000

binary_prefix = "%s/build" % os.getcwd()


class RemoteProcess:
    def __init__(self, host, command):
        self.host = host
        self.command = command
        self.remote = None

    def start(self):
        self.remote = subprocess.Popen(["ssh", self.host, self.command])
        print("start binary on %s with %s" % (self.host, self.command))

    def kill(self):
        self.remote.kill()
        print("stop host %s" % self.host)

    def wait(self):
        self.remote.wait()


class Server(RemoteProcess):
    def __init__(self, host):
        command = "%s/%s -l %s:%d" % (binary_prefix, server_binary, host, port)
        RemoteProcess.__init__(self, host, command)


class KillServer(RemoteProcess):
    def __init__(self, host):
        command = "killall %s" % server_binary
        RemoteProcess.__init__(self, host, command)


class Client(RemoteProcess):
    def __init__(self, host):
        command = "%s/%s -c %s:%d" % (binary_prefix, client_binary, server_host, port)
        RemoteProcess.__init__(self, host, command)


if __name__ == '__main__':
    server = Server(server_host)
    server.start()

    time.sleep(2)

    clients = []
    for client_host in client_hosts:
        client = Client(client_host)
        client.start()
        clients.append(client)

    for client in clients:
        client.wait()

    kill_server = KillServer(server_host)
    kill_server.start()
    kill_server.wait()

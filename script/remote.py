import subprocess
import os

server_binary = "gungnir"

binary_prefix = "%s/build" % os.getcwd()

port = 3000


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
    def __init__(self, host, binary, server_host):
        command = "%s/%s -c %s:%d" % (binary_prefix, binary, server_host, port)
        RemoteProcess.__init__(self, host, command)

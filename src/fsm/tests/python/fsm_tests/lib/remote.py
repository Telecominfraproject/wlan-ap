import logging
import os
import paramiko
import subprocess


class Remote:
    def __init__(self, remote):
        self.remote = remote
        self.username = 'root'
        self.dir_path = os.path.dirname(os.path.realpath(__file__))
        self.fix_script = os.path.join(self.dir_path, 'clean_ssh.sh')
        self.client = self.connect()
        if self.client is None:
            self.attempt_fix()
            self.client = self.connect()
            if self.client is None:
                raise Exception("ssh access repair to %s failed" %
                                self.remote)

    def connect(self):
        client = paramiko.client.SSHClient()
        try:
            client.load_system_host_keys()
            client.connect(self.remote, username=self.username, timeout=10)
        except:
            error_str = "could not connect to %s. "
            error_str += "Will attempt to fix the situation"
            logging.warning(error_str % self.remote)
            return
        return client

    def attempt_fix(self):
        subprocess.call([self.fix_script, self.remote])

    def run_command(self, command):
        return self.client.exec_command(command)

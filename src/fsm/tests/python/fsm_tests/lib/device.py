import re


class Device:
    def __init__(self, mac_addr):
        self.mac_addr = mac_addr
        self.valid = self.validate()

    def validate(self):
        if len(self.mac_addr) != 17:
            return False

        pattern = r'[a-fA-F0-9]{2}[:][a-fA-F0-9]{2}[:][a-fA-F0-9]{2}[:]'
        pattern += r'[a-fA-F0-9]{2}[:][a-fA-F0-9]{2}[:][a-fA-F0-9]{2}'
        m = re.compile(pattern)
        return (m.match(self.mac_addr) is not None)

import os

PORT = 50051
PREFIX = ""
fake_device = f"/dev/sdc1"


def pad_to_4096(data: str):
    return data + "\x00" * (4096 - len(data))


MAGIC = {}
test_dir = os.path.dirname(os.path.realpath(__file__))
with open(os.path.join(test_dir, "..", "magic.h")) as f:
    for line in f.readlines():
        line = line.replace("#define ", "").split(" ", 1)
        MAGIC[line[0]] = line[1].split('"')[1]

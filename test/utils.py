def pad_to_4096(data: str):
    return data + "\x00" * (4096 - len(data))


MAGIC = {}
with open("../magic.h") as f:
    for line in f.readlines():
        line = line.replace("#define ", "").split(" ", 1)
        MAGIC[line[0]] = line[1].split('"')[1]

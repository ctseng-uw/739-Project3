import asyncssh
import logging


class Client:
    def __init__(self, conn: asyncssh.SSHClientConnection):
        self.conn = conn

    async def write(self, target: int, addr: int, data: str):
        logging.info(f"Client write at {addr}")
        await self.conn.run(f"/tmp/client {target} w {addr} {data}", check=True)

    async def read(self, target: int, addr: int):
        logging.info(f"Client read at {addr}")
        client_proc = await self.conn.run(f"/tmp/client {target}  r {addr}", check=True)
        return client_proc.stdout

    def run(self, cmd: str, input: str = None):
        logging.info(cmd)
        return self.conn.run(cmd, check=True, input=input)

    async def close(self):
        await self.conn.run("pkill mfsfuse")
        await self.conn.run("sudo umount /tmp/go")
        return self.conn.close()

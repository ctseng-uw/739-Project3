import asyncssh
import logging
from .utils import PREFIX


class Client:
    def __init__(self, conn: asyncssh.SSHClientConnection):
        self.conn = conn

    async def write(
        self, target: int, addr: int, data: str
    ) -> asyncssh.SSHCompletedProcess:
        logging.info(f"Client write to node{target} at {addr}")
        client_proc = await self.conn.run(
            f"/tmp/{PREFIX}client {target} w {addr} {data}", check=True
        )
        return client_proc

    async def write_any(self, addr: int, data: str) -> asyncssh.SSHCompletedProcess:
        logging.info(f"Client write to either node at {addr}")
        client_proc = await self.conn.run(
            f"/tmp/{PREFIX}client w {addr} {data}", check=True
        )
        return client_proc

    async def read(self, target: int, addr: int) -> asyncssh.SSHCompletedProcess:
        logging.info(f"Client read at {addr}")
        client_proc = await self.conn.run(
            f"/tmp/{PREFIX}client {target}  r {addr}", check=True
        )
        return client_proc

    async def read_any(self, addr: int) -> asyncssh.SSHCompletedProcess:
        logging.info(f"Client read from any at {addr}")
        client_proc = await self.conn.run(f"/tmp/{PREFIX}client r {addr}", check=True)
        return client_proc

    def run(self, cmd: str, input: str = None):
        logging.info(cmd)
        return self.conn.run(cmd, check=True, input=input)

    async def close(self):
        await self.conn.run(f"pkill {PREFIX}mfsfuse")
        await self.conn.run(f"sudo umount /tmp/{PREFIX}go")
        return self.conn.close()

    async def setup_fuse(self):
        await self.conn.run(f"mkdir -p /tmp/{PREFIX}go")
        await self.conn.run(f"/tmp/{PREFIX}mfsfuse -s /tmp/{PREFIX}go")

    async def run_sql(self, sqlcmd: str):
        return await self.conn.run(f"sqlite /tmp/{PREFIX}go/test.sql", input=sqlcmd)

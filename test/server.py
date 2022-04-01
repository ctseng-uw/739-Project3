from typing import Optional
import asyncssh
import logging
from .utils import *


class Server:
    def __init__(self, conn: asyncssh.SSHClientConnection, node_number):
        self.conn = conn
        self.node_number = node_number
        self.proc: Optional[asyncssh.SSHClientProcess] = None

    async def start_server(self):
        assert self.proc is None
        self.proc = await self.conn.create_process(
            f"/tmp/server {self.node_number} {1 - self.node_number}"
        )
        await self.proc.stdout.readuntil(MAGIC["MAGIC_SERVER_START"])
        logging.info(f"Server started at node {self.node_number}")

    async def recovery_complete(self):
        await self.proc.stdout.readuntil(MAGIC["MAGIC_RECOVERY_COMPLETE"])

    async def get_device_digest(self):
        proc = await self.conn.run("sha256sum /tmp/fake_device.bin")
        return proc.stdout

    async def close(self):
        await self.conn.run("pkill server")
        await self.conn.run("cp /tmp/fake_device.bin /tmp/fake_device.bin.bk")
        await self.conn.run("rm /tmp/fake_device.bin")
        return self.conn.close()

    async def commit_suicide(self):
        assert self.proc is not None
        self.proc.terminate()
        self.proc = None

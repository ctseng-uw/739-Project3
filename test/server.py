from typing import Optional
import asyncssh
import logging
from .utils import *


class Server:
    def __init__(self, conn: asyncssh.SSHClientConnection, node_number):
        self.conn = conn
        self.node_number = node_number
        self.proc: Optional[asyncssh.SSHClientProcess] = None

    async def __start_server(self, start_as_primary):
        assert self.proc is None
        self.proc = await self.conn.create_process(
            f"/tmp/server {self.node_number} {1 if start_as_primary else 0}"
        )
        await self.proc.stdout.readuntil(MAGIC["MAGIC_SERVER_START"])
        logging.info(
            f"Server started at node {self.node_number} ({'Primary' if start_as_primary else 'Backup'})"
        )

    def start_as_primary(self):
        return self.__start_server(True)

    def start_as_backup(self):
        return self.__start_server(False)

    async def recovery_complete(self):
        await self.proc.stdout.readuntil(MAGIC["MAGIC_RECOVERY_COMPLETE"])
        logging.info(f"Server {self.node_number} recovery complete")

    async def become_primary(self):
        await self.proc.stdout.readuntil(MAGIC["MAGIC_BECOMES_PRIMARY"])
        logging.info(f"Server {self.node_number} becomes primary")

    async def get_device_digest(self):
        proc = await self.conn.run("sha256sum /tmp/fake_device.bin")
        return proc.stdout

    async def close(self):
        await self.conn.run("pkill server")
        await self.conn.run("mv /tmp/fake_device.bin /tmp/fake_device.bin.bk")
        return self.conn.close()

    async def commit_suicide(self):
        assert self.proc is not None
        self.proc.terminate()
        logging.info(f"Server {self.node_number} commits suicide")
        self.proc = None

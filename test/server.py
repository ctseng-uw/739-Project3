import asyncio
from typing import Optional
import asyncssh
import logging
from .utils import MAGIC, PREFIX, PORT, fake_device


class Server:
    def __init__(self, conn: asyncssh.SSHClientConnection, node_number):
        self.conn = conn
        self.node_number = node_number
        self.proc: Optional[asyncssh.SSHClientProcess] = None

    async def __start_server(self, start_as_primary):
        assert self.proc is None
        self.proc = await self.conn.create_process(
            f"sudo /tmp/{PREFIX}server {self.node_number} {1 if start_as_primary else 0}"
        )
        await self.proc.stdout.readuntil(MAGIC["MAGIC_SERVER_START"])
        logging.info(
            f"Server started at node {self.node_number} ({'Primary' if start_as_primary else 'Backup'})"
        )

    def start_as_primary(self):
        return self.__start_server(True)

    def start_as_backup(self):
        return self.__start_server(False)

    async def starting_recovery(self):
        await self.proc.stdout.readuntil(MAGIC["MAGIC_STARTING_RECOVERY"])
        logging.info(f"Server {self.node_number} starting recovery")

    async def recovery_complete(self):
        await self.proc.stdout.readuntil(MAGIC["MAGIC_RECOVERY_COMPLETE"])
        logging.info(f"Server {self.node_number} recovery complete")

    async def become_primary(self):
        await self.proc.stdout.readuntil(MAGIC["MAGIC_BECOMES_PRIMARY"])
        logging.info(f"Server {self.node_number} becomes primary")

    async def get_device_digest(self):
        proc = await self.conn.run(
            f"sudo head -c {512 * 1024 * 1024}  {fake_device} | sha256sum"
        )
        return proc.stdout

    async def close(self):
        await self.conn.run(f"sudo pkill -x {PREFIX}server")
        return self.conn.close()

    async def lan_down(self, link_name: str):
        await self.conn.run(
            f"sudo iptables -A OUTPUT -o {link_name} -p tcp --destination-port {PORT} -j DROP"
        )
        await self.conn.run(
            f"sudo iptables -A INPUT -i {link_name} -p tcp --destination-port {PORT} -j DROP"
        )
        logging.info(f"Server {self.node_number} LAN down")

    async def lan_up(self, link_name: str):
        await self.conn.run(f"sudo iptables -F")
        logging.info(f"Server {self.node_number} LAN up")

    async def commit_suicide(self):
        assert self.proc is not None
        await self.conn.run(f"sudo pkill -9 -x {PREFIX}server")
        logging.info(f"Server {self.node_number} commits suicide")
        self.proc = None

    def collect_output(self):
        out, _ = self.proc.collect_output()
        logging.info(out)

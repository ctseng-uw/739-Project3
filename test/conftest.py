import pytest
import asyncssh
import asyncio
import logging
from typing import List
from .client import Client
from .server import Server
from .utils import PREFIX
import os

server_addrs = ["node0", "node1"]
server_external_addrs = [
    "node0.hadev3.advosuwmadison-pg0.wisc.cloudlab.us",
    "node1.hadev3.advosuwmadison-pg0.wisc.cloudlab.us",
]
client_addrs = [
    "node2.hadev3.advosuwmadison-pg0.wisc.cloudlab.us",
    "node3.hadev3.advosuwmadison-pg0.wisc.cloudlab.us",
]
linkname = "enp6s0f0"

test_dir = os.path.dirname(os.path.realpath(__file__))


@pytest.fixture(scope="session", autouse=True)
async def compile():
    asyncssh.set_log_level(100)

    async def run_in_build(cmd):
        proc = await asyncio.create_subprocess_shell(
            cmd,
            cwd=os.path.join(test_dir, "..", "build"),
            stdout=asyncio.subprocess.PIPE,
        )
        await proc.communicate()
        assert proc.returncode == 0

    await run_in_build("cmake ..")
    await run_in_build("make -j")
    await asyncio.create_subprocess_shell(f"sudo ip-tables -F")
    promises = []
    for s in server_addrs:
        promises.append(
            run_in_build(
                f"scp -o 'StrictHostKeyChecking=no' server {s}:/tmp/{PREFIX}server"
            )
        )
    for c in client_addrs:
        promises.append(
            run_in_build(
                f"scp -o 'StrictHostKeyChecking=no' client {c}:/tmp/{PREFIX}client"
            )
        )
        promises.append(
            run_in_build(
                f"scp -o 'StrictHostKeyChecking=no' mfsfuse {c}:/tmp/{PREFIX}mfsfuse"
            )
        )
    await asyncio.wait(promises)
    logging.info("Compile Done")


@pytest.fixture(scope="session")
def event_loop():
    return asyncio.get_event_loop()


@pytest.fixture
async def clients():
    ret = []
    for node in client_addrs:
        conn = await asyncssh.connect(node)
        ret.append(Client(conn))
    yield ret
    for c in ret:
        await c.close()


@pytest.fixture
async def servers():
    ret = []
    for idx, node in enumerate(server_addrs):
        conn = await asyncssh.connect(node)
        await conn.run(f"sudo pkill -x {PREFIX}server")
        await conn.run("sudo dd if=/dev/zero of=/dev/sdc1 bs=1G count=1")
        ret.append(Server(conn, idx))
    yield ret
    for s in ret:
        await s.close()


@pytest.fixture
async def servers_external():
    ret = []
    for idx, node in enumerate(server_external_addrs):
        conn = await asyncssh.connect(node)
        await conn.run(f"sudo pkill -x {PREFIX}server")
        await conn.run("sudo dd if=/dev/zero of=/dev/sdc1 bs=1G count=1")
        ret.append(Server(conn, idx))
    yield ret
    for s in ret:
        await s.close()


@pytest.fixture
async def link_name():
    return linkname


@pytest.fixture
async def setup_two_servers(servers: List[Server]):
    [primary, backup] = servers
    logging.info(f"primary={primary.conn._host}, backup={backup.conn._host}")
    await primary.start_as_primary()
    await backup.start_as_backup()
    await primary.recovery_complete()


@pytest.fixture
async def setup_two_servers_external(servers_external: List[Server]):
    [primary, backup] = servers_external
    logging.info(f"primary={primary.conn._host}, backup={backup.conn._host}")
    await primary.start_as_primary()
    await backup.start_as_backup()
    await primary.recovery_complete()

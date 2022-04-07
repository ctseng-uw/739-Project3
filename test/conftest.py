import pytest
import asyncssh
import asyncio
import logging
import subprocess
from typing import List
from .client import Client
from .server import Server
from .utils import PREFIX, fake_device
import os

server_addrs = [
    "node0.hadev3.advosuwmadison-pg0.wisc.cloudlab.us",
    "node1.hadev3.advosuwmadison-pg0.wisc.cloudlab.us",
]
client_addrs = [
    "node2.hadev3.advosuwmadison-pg0.wisc.cloudlab.us",
    "node3.hadev3.advosuwmadison-pg0.wisc.cloudlab.us",
]
linkname = "enp6s0f0"

test_dir = os.path.dirname(os.path.realpath(__file__))
build_dir = os.path.join(test_dir, "..", "build")

asyncssh.set_log_level(100)


@pytest.fixture(scope="session", autouse=True)
async def compile():
    asyncssh.set_log_level(100)

    def run_in_build(cmd):
        subprocess.run(cmd, cwd=build_dir, check=True, shell=True)

    run_in_build("cmake ..")
    run_in_build("make -j")
    promises = []
    for s in server_addrs:
        promises.append(
            asyncssh.scp(os.path.join(build_dir, "server"), f"{s}:/tmp/{PREFIX}server")
        )
    for c in client_addrs:
        promises += [
            asyncssh.scp(os.path.join(build_dir, "client"), f"{c}:/tmp/{PREFIX}client"),
            asyncssh.scp(
                os.path.join(build_dir, "mfsfuse"), f"{c}:/tmp/{PREFIX}mfsfuse"
            ),
        ]
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
async def client(clients):
    yield clients[0]


@pytest.fixture
async def servers():
    ret = []
    for idx, node in enumerate(server_addrs):
        conn = await asyncssh.connect(node)
        await conn.run(f"sudo pkill -f {PREFIX}server")
        await conn.run(f"sudo dd if=/dev/zero of={fake_device} bs=1G count=1")
        await conn.run(f"sudo iptables -F")
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
async def setup_two_servers_1_as_primary(servers: List[Server]):
    [backup, primary] = servers
    logging.info(f"primary={primary.conn._host}, backup={backup.conn._host}")
    await primary.start_as_primary()
    await backup.start_as_backup()
    await primary.recovery_complete()

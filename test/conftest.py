import pytest
import asyncssh
import asyncio
import logging
from typing import List
from .client import Client
from .server import Server

server_addrs = ["node0", "node1"]
# server_external_addrs = ["node0.hadev3.advosuwmadison-pg0.wisc.cloudlab.us", "node1.hadev3.advosuwmadison-pg0.wisc.cloudlab.us"]
server_external_addrs = ["node0.hadev2.advosuwmadison.emulab.net", "node1.hadev2.advosuwmadison.emulab.net"]
client_addrs = ["node2", "node3"]
linkname = "enp66s0f0"


@pytest.fixture(scope="session", autouse=True)
async def compile():
    asyncssh.set_log_level(100)

    async def run_in_build(cmd):
        proc = await asyncio.create_subprocess_shell(cmd, cwd="../build")
        await proc.communicate()
        assert proc.returncode == 0

    await run_in_build("cmake ..")
    await run_in_build("make -j")
    promises = []
    for s in server_addrs:
        promises.append(
            run_in_build(f"scp -o 'StrictHostKeyChecking=no' server {s}:/tmp/server")
        )
    for c in client_addrs:
        promises.append(
            run_in_build(f"scp -o 'StrictHostKeyChecking=no' client {c}:/tmp/client")
        )
        promises.append(
            run_in_build(f"scp -o 'StrictHostKeyChecking=no' mfsfuse {c}:/tmp/mfsfuse")
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
        ret.append(Server(conn, idx))
    yield ret
    for s in ret:
        await s.close()

@pytest.fixture
async def servers_external():
    ret = []
    for idx, node in enumerate(server_external_addrs):
        conn = await asyncssh.connect(node)
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

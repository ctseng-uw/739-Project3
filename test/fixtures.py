import logging
from .client import Client
from .server import Server
import asyncssh
import pytest
import asyncio
from typing import List

server_addrs = ["node0", "node1"]
client_addrs = ["node2", "node3"]


@pytest.fixture(scope="session", autouse=True)
async def compile():
    asyncssh.set_log_level(100)

    async def run_in_build(cmd):
        proc = await asyncio.create_subprocess_shell(
            cmd, cwd="../build", stdout=asyncio.subprocess.PIPE
        )
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

import logging
from .client import Client
from .server import Server
import asyncssh
import pytest
import asyncio
from typing import List
from .conftest import server_addrs, client_addrs


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

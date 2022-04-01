from typing import List

from .utils import *
from .fixtures import *
from .client import Client
from .server import Server


async def test_single_block(servers: List[Server], clients: List[Client]):
    await servers[0].start_server()
    await clients[0].write(0, "apple")
    assert await clients[0].read(0) == pad_to_4096("apple")


async def test_unaligned_block(servers: List[Server], clients: List[Client]):
    await servers[0].start_server()
    await clients[0].write(0, "apple")
    await clients[0].write(5, "banana")
    assert await clients[0].read(0) == pad_to_4096("applebanana")
    assert await clients[0].read(5) == pad_to_4096("banana")


async def test_two_servers(servers: List[Server], clients: List[Client]):
    primary = servers[0]
    backup = servers[1]
    await primary.start_server()
    await backup.start_server()
    await primary.recovery_complete()
    for i in range(5):
        await clients[0].write(4096 * i, "apple")
    assert await backup.get_device_digest() == await primary.get_device_digest()
    await clients[0].read(0) == pad_to_4096("apple")

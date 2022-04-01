from typing import List

from .utils import *
from .client import Client
from .server import Server


async def test_single_block(servers: List[Server], clients: List[Client]):
    await servers[0].start_as_backup()
    await clients[0].write(0, 0, "apple")
    assert await clients[0].read(0, 0) == pad_to_4096("apple")


async def test_unaligned_block(servers: List[Server], clients: List[Client]):
    await servers[0].start_as_backup()
    await clients[0].write(0, 0, "apple")
    await clients[0].write(0, 5, "banana")
    assert await clients[0].read(0, 0) == pad_to_4096("applebanana")
    assert await clients[0].read(0, 5) == pad_to_4096("banana")


async def test_two_servers(
    servers: List[Server], clients: List[Client], setup_two_servers: None
):
    for i in range(5):
        await clients[0].write(0, 4096 * i, "apple")
    assert await servers[0].get_device_digest() == await servers[1].get_device_digest()
    await clients[0].read(0, 0) == pad_to_4096("apple")

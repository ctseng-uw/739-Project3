from typing import List
from .utils import *
from .client import Client
from .server import Server


async def test_primary_dead_simple(
    servers: List[Server], clients: List[Client], setup_two_servers: None
):
    [primary, backup] = servers
    client = clients[0]
    await client.write(0, 0, "123456")
    assert (await client.read(0, 0)).stdout == pad_to_4096("123456")
    await primary.commit_suicide()
    await backup.become_primary()
    await client.write(1, 6, "789")
    assert (await client.read(1, 0)).stdout == pad_to_4096("123456789")


async def test_primary_dead_up_again(
    servers: List[Server], clients: List[Client], setup_two_servers: None
):
    [primary, backup] = servers
    client = clients[0]
    await client.write(0, 0, "123456")
    assert (await client.read(0, 0)).stdout == pad_to_4096("123456")
    await primary.commit_suicide()
    await backup.become_primary()
    await client.write(1, 6, "789")
    assert (await client.read(1, 0)).stdout == pad_to_4096("123456789")
    await client.write(1, 4096, "banana")
    await primary.start_as_backup()
    await backup.recovery_complete()
    assert (await client.read(1, 4096)).stdout == pad_to_4096("banana")
    assert await primary.get_device_digest() == await backup.get_device_digest()

import asyncio
import logging
from typing import List
from .client import Client
from .server import Server
from .utils import *
import random, string


async def test_backup_dead_simple(
    servers: List[Server], clients: List[Client], setup_two_servers: None
):
    backup = servers[1]
    primary = servers[0]
    await clients[0].write(0, 0, "apple")
    assert await backup.get_device_digest() == await primary.get_device_digest()
    await backup.terminate()
    ret = await clients[0].write(0, 0, "banana")
    await clients[0].write(0, 6, "guava")
    assert (await clients[0].read(0, 0)).stdout == pad_to_4096("bananaguava")
    assert await backup.get_device_digest() != await primary.get_device_digest()
    await backup.start_as_backup()
    await primary.recovery_complete()
    assert await backup.get_device_digest() == await primary.get_device_digest()


async def test_backup_die_again_during_recovery(
    servers: List[Server], clients: List[Client], setup_two_servers: None
):
    backup = servers[1]
    primary = servers[0]
    await clients[0].write(0, 0, "helloworld!")
    assert await backup.get_device_digest() == await primary.get_device_digest()
    await backup.terminate()
    for addr in range(500):
        await clients[0].write(
            0,
            addr * 4096,
            "".join(random.choice(string.ascii_lowercase) for i in range(20)),
        )
    await clients[0].write(
        0,
        100 * 4096,
        "teststring",
    )
    assert (await clients[0].read(0, 100 * 4096)).stdout == pad_to_4096("teststring")
    prev_digest = await backup.get_device_digest()
    await backup.start_as_backup()
    await primary.starting_recovery()
    await backup.terminate()
    await primary.recovery_complete()
    assert (
        await primary.get_device_digest() != await backup.get_device_digest()
    )  # make sure recovery does not complete
    assert (
        await backup.get_device_digest() != prev_digest
    )  # make sure we did part of the recovery
    await backup.start_as_backup()
    await primary.recovery_complete()
    assert await backup.get_device_digest() == await primary.get_device_digest()
    assert (await clients[0].read(0, 100 * 4096)).stdout == pad_to_4096("teststring")

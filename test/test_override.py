from typing import List
from .client import Client
from .server import Server
from .utils import *


async def test_backup_dead_overwrite(
    servers: List[Server], clients: List[Client], setup_two_servers: None
):
    backup = servers[1]
    primary = servers[0]
    await clients[0].write(0, 0, "apple")
    assert await backup.get_device_digest() == await primary.get_device_digest()
    await backup.commit_suicide()
    for i in range(20):
        await clients[0].write(0, 0, str(i))
    assert (await clients[0].read(0, 0)).stdout == pad_to_4096("19")
    assert await backup.get_device_digest() != await primary.get_device_digest()
    await backup.start_as_backup()
    await primary.recovery_complete()
    assert await backup.get_device_digest() == await primary.get_device_digest()
    await primary.commit_suicide()
    await backup.become_primary()
    assert (await clients[0].read(1, 0)).stdout == pad_to_4096("19")

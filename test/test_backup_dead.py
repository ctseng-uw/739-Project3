from typing import List
from .utils import *
from .fixtures import *


async def test_backup_dead(servers: List[Server], clients: List[Client]):
    backup = servers[1]
    primary = servers[0]
    await primary.start_server()
    await backup.start_server()
    await primary.recovery_complete()
    await clients[0].write(0, "apple")
    assert await backup.get_device_digest() == await primary.get_device_digest()
    await backup.commit_suicide()
    await clients[0].write(0, "banana")
    await clients[0].write(6, "guava")
    assert await clients[0].read(0) == pad_to_4096("bananaguava")
    assert await backup.get_device_digest() != await primary.get_device_digest()
    await backup.start_server()
    await primary.recovery_complete()
    assert await backup.get_device_digest() == await primary.get_device_digest()

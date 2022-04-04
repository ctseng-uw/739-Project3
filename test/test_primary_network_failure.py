from typing import List

import pytest
from .utils import *
from .client import Client
from .server import Server
from asyncssh.process import ProcessError


# async def test_write_to_backup(
#     servers_external: List[Server], clients: List[Client], setup_two_servers: None
# ):
#     [primary, backup] = servers_external
#     client = clients[0]
#     await client.write(0, 8192, "write_to_node0")
#     with pytest.raises(ProcessError):
#         assert (await client.write(1, 8192, "write_to_node1"))[0] == "This server is backup"
#     assert (await client.read_any(8192))[1] == pad_to_4096("write_to_node0")
#     with pytest.raises(ProcessError):
#         assert (await client.read(1, 8192))[0] == "This server is backup"

async def test_primary_network_down(
    servers_external: List[Server], clients: List[Client], link_name: str, setup_two_servers: None
):
    [node0, node1] = servers_external
    client = clients[0]
    await client.write(0, 8192, "write_to_node0")
    await node0.lan_down(link_name)
    await node1.become_primary()
    assert (await client.read_any(8192)) == ("Read from node1", pad_to_4096("write_to_node0"))
    assert (await client.write_any(8192, "change_when_node0_out_of_reach"))[0] == "Write to node1"
    await node0.lan_up(link_name)
    await node1.recovery_complete()
    with pytest.raises(ProcessError):
        assert await client.read(0, 8192).stderr == "This server is backup"
    # node0 knows itself is backup



# async def test_primary_dead_up_again(
#     servers: List[Server], clients: List[Client], setup_two_servers: None
# ):
#     [primary, backup] = servers
#     client = clients[0]
#     await client.write(0, 0, "123456")
#     assert await client.read(0, 0) == pad_to_4096("123456")
#     await primary.commit_suicide()
#     await backup.become_primary()
#     await client.write(1, 6, "789")
#     assert await client.read(1, 0) == pad_to_4096("123456789")
#     await client.write(1, 4096, "banana")
#     await primary.start_as_backup()
#     await backup.recovery_complete()
#     assert await client.read(1, 4096) == pad_to_4096("banana")
#     assert await primary.get_device_digest() == await backup.get_device_digest()

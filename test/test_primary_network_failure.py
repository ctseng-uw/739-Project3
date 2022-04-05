from typing import List

import pytest
from .utils import *
from .client import Client
from .server import Server
from asyncssh.process import ProcessError


async def test_write_to_backup(
    servers_external: List[Server],
    clients: List[Client],
    setup_two_servers_external: None,
):
    client = clients[0]
    await client.write(0, 8192, "write_to_node0")
    with pytest.raises(ProcessError):
        assert (
            await client.write(1, 8192, "write_to_node1")
        ).stderr == "This server is backup"
    assert (await client.read_any(8192)).stdout == pad_to_4096("write_to_node0")
    with pytest.raises(ProcessError):
        assert (await client.read(1, 8192)).stderr == "This server is backup"


async def test_primary_network_down(
    servers_external: List[Server],
    clients: List[Client],
    link_name: str,
    setup_two_servers_external: None,
):
    [node0, node1] = servers_external
    client = clients[0]
    await client.write(0, 8192, "write_to_node0")
    await node0.lan_down(link_name)
    await node1.become_primary()
    ret = await client.read_any(8192)
    assert ret.stderr == "Read from node1"
    assert ret.stdout == pad_to_4096("write_to_node0")
    assert (
        await client.write_any(8192, "change_when_node0_out_of_reach")
    ).stderr == "Write to node1"
    await node0.lan_up(link_name)
    await node1.recovery_complete()
    with pytest.raises(ProcessError):
        assert (await client.read(0, 8192)).stderr == "This server is backup"
    # node0 knows itself is backup

from typing import List

import pytest
from .utils import *
from .client import Client
from .server import Server
from asyncssh.process import ProcessError


async def test_write_to_backup(
    servers: List[Server],
    clients: List[Client],
    setup_two_servers: None,
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


async def test_network_down(
    servers: List[Server],
    client: Client,
    setup_two_servers_1_as_primary: None,
    link_name,
):
    await client.setup_fuse()
    sql_create_insert = """
        CREATE TABLE coffees (
        id INTEGER PRIMARY KEY,
        coffee_name TEXT NOT NULL,
        price REAL NOT NULL
        );

        INSERT INTO coffees VALUES (null, 'Colombian', 7.99);
        INSERT INTO coffees VALUES (null, 'French_Roast', 8.99);
        INSERT INTO coffees VALUES (null, 'Espresso', 9.99);
        INSERT INTO coffees VALUES (null, 'Colombian_Decaf', 8.99);
        INSERT INTO coffees VALUES (null, 'French_Roast_Decaf', 9.99);
    """
    coroutine = client.run_sql(sql_create_insert)
    await servers[1].lan_down(link_name)
    sql_select = """
    select * from coffees;
    """
    await coroutine
    ret = await client.run_sql(sql_select)
    assert "4|Colombian_Decaf|8.99" in ret.stdout
    assert await servers[0].get_device_digest() == await servers[1].get_device_digest()

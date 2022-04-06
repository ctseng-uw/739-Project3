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


async def test_die_mid_sqlite(
    servers: List[Server], clients: List[Client], setup_two_servers: None
):
    sqlcmd = """
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
    await clients[0].run(f"mkdir -p /tmp/{PREFIX}go")
    await clients[0].run(f"/tmp/{PREFIX}mfsfuse -s /tmp/{PREFIX}go")
    sqlrun = clients[0].run(f"sqlite /tmp/{PREFIX}go/test.sql", sqlcmd)
    await servers[0].commit_suicide()
    await sqlrun
    sqlcmd = """
    select * from coffees;
    """
    ret = await clients[0].run(f"sqlite /tmp/{PREFIX}go/test.sql", sqlcmd)
    assert "4|Colombian_Decaf|8.99" in ret.stdout
    await servers[0].start_as_backup()
    await servers[1].recovery_complete()
    assert await servers[0].get_device_digest() == await servers[1].get_device_digest()


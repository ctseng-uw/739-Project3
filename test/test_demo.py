import random
import string
from typing import List

from .utils import *
from .client import Client
from .server import Server


async def test_demo_1(
    servers: List[Server], client: Client, setup_two_servers_1_as_primary: None
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
    await servers[1].commit_suicide()
    sql_select = """
    select * from coffees;
    """
    await coroutine
    ret = await client.run_sql(sql_select)
    assert "4|Colombian_Decaf|8.99" in ret.stdout
    await servers[1].start_as_backup()
    await servers[0].recovery_complete()
    assert await servers[0].get_device_digest() == await servers[1].get_device_digest()


async def test_demo_2(
    servers: List[Server], clients: List[Client], setup_two_servers: None
):
    backup = servers[1]
    primary = servers[0]
    await clients[0].write(0, 0, "helloworld!")
    assert await backup.get_device_digest() == await primary.get_device_digest()
    await backup.commit_suicide()
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
    await backup.commit_suicide()
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

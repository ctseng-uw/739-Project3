import asyncio
import logging
import random
import string
from typing import List

from .utils import *
from .client import Client
from .server import Server


def get_random_string():
    return "".join(random.choice(string.ascii_lowercase) for i in range(20))


async def test_demo_1(
    servers: List[Server], client: Client, setup_two_servers_1_as_primary: None
):
    await client.setup_fuse()
    logging.info("Fuse setup completed")

    sql_create_insert = """
        CREATE TABLE coffees (
        id INTEGER PRIMARY KEY,
        coffee_name TEXT NOT NULL,
        price REAL NOT NULL
        );

        INSERT INTO coffees VALUES (null, 'Colombian', 7.99);
        INSERT INTO coffees VALUES (null, 'French_Roast', 8.99);
        INSERT INTO coffees VALUES (null, 'Colombian_Decaf', 8.99);
    """
    future = asyncio.ensure_future(client.run_sql(sql_create_insert))
    logging.info("SQL Create Insert commands dispatched")
    await servers[1].wait_for_some_write()
    await servers[1].terminate()
    logging.info("Sever1 terminated")

    await future
    logging.info("SQL Create Insert commands completed")
    sql_select = "select * from coffees;"
    ret = await client.run_sql(sql_select)
    logging.info(f"SQL Select returned {ret.stdout}")

    assert "3|Colombian_Decaf|8.99" in ret.stdout
    await servers[1].start_as_backup()
    await servers[0].recovery_complete()
    logging.info("Server1 restarted and recovery sequence is completed")
    assert await servers[0].get_device_digest() == await servers[1].get_device_digest()
    logging.info("Data on server0 and server1 are the same")


async def test_demo_2(
    servers: List[Server], clients: List[Client], setup_two_servers_1_as_primary: None
):
    logging.info("Setup completed")
    [backup, primary] = servers
    await backup.terminate()
    logging.info("Backup terminated")

    logging.info("Start to write blocks")
    for idx in range(500):
        await clients[0].write(
            1,
            idx * 4096,
            f"wrote_some_data_at_{idx}",
        )
    logging.info("Finish writing block")

    logging.info("Restart Backup")
    await backup.start_as_backup()
    await primary.starting_recovery()
    await backup.terminate()
    logging.info("Backup terminated again")
    await primary.recovery_complete()

    assert (
        await primary.get_device_digest() != await backup.get_device_digest()
    )  # make sure recovery does not complete

    logging.info("Restart Backup again")
    await backup.start_as_backup()
    await primary.recovery_complete()
    logging.info("Recovery Complete")

    assert await backup.get_device_digest() == await primary.get_device_digest()
    logging.info("Data on server0 and server1 are the same")
    read_result = (await clients[0].read(1, 300 * 4096)).stdout
    assert read_result == pad_to_4096(f"wrote_some_data_at_300")
    logging.info("Data read is as expected")

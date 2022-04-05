from typing import List
from .utils import *
from .client import Client
from .server import Server
from .utils import PREFIX


async def test_hello_fuse(
    servers: List[Server], clients: List[Client], setup_two_servers: None
):
    await clients[0].run(f"mkdir -p /tmp/{PREFIX}go")
    await clients[0].run(f"/tmp/{PREFIX}mfsfuse -s /tmp/{PREFIX}go")
    await clients[0].run(f"echo -n 'hello world' > /tmp/{PREFIX}hello.txt")
    await clients[0].run(f"cp /tmp/{PREFIX}hello.txt /tmp/{PREFIX}go")
    ret = await clients[0].run(f"cat /tmp/{PREFIX}go/{PREFIX}hello.txt")
    assert ret.stdout == "hello world"
    assert await servers[0].get_device_digest() == await servers[1].get_device_digest()


async def test_sqlite(
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
    await clients[0].run(f"sqlite /tmp/{PREFIX}go/test.sql", sqlcmd)
    sqlcmd = """
    select * from coffees;
    """
    ret = await clients[0].run(f"sqlite /tmp/{PREFIX}go/test.sql", sqlcmd)
    assert "4|Colombian_Decaf|8.99" in ret.stdout
    assert await servers[0].get_device_digest() == await servers[1].get_device_digest()

from typing import List
from .utils import *
from .fixtures import *


async def test_hello_fuse(servers: List[Server], clients: List[Client]):
    await servers[0].start_server()
    await servers[1].start_server()
    await servers[0].recovery_complete()
    await clients[0].run("mkdir -p /tmp/go")
    await clients[0].run("/tmp/mfsfuse -s /tmp/go")
    await clients[0].run("echo -n 'hello world' > /tmp/hello.txt")
    await clients[0].run("cp /tmp/hello.txt /tmp/go")
    ret = await clients[0].run("cat /tmp/go/hello.txt")
    assert ret.stdout == "hello world"
    assert await servers[0].get_device_digest() == await servers[1].get_device_digest()


async def test_sqlite(servers: List[Server], clients: List[Client]):
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
    await servers[0].start_server()
    await servers[1].start_server()
    await servers[0].recovery_complete()
    await clients[0].run("mkdir -p /tmp/go")
    await clients[0].run("/tmp/mfsfuse -s /tmp/go")
    await clients[0].run("sqlite /tmp/go/test.sql", sqlcmd)
    sqlcmd = """
    select * from coffees;
    """
    ret = await clients[0].run("sqlite /tmp/go/test.sql", sqlcmd)
    assert "4|Colombian_Decaf|8.99" in ret.stdout
    assert await servers[0].get_device_digest() == await servers[1].get_device_digest()

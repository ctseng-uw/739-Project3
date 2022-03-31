import pytest
import asyncssh
import asyncio
import logging

server_addrs = ["node0", "node1"]
client_addrs = ["node2", "node3"]

@pytest.fixture(scope="session", autouse=True)
async def compile():
    asyncssh.set_log_level(100)

    async def run_in_build(cmd):
        proc = await asyncio.create_subprocess_shell(
            cmd, cwd="../build"
        )
        await proc.communicate()
        assert proc.returncode == 0

    await run_in_build("cmake ..")
    await run_in_build("make -j")
    promises = []
    for s in server_addrs:
        promises.append(
            run_in_build(f"scp -o 'StrictHostKeyChecking=no' server {s}:/tmp/server")
        )
    for c in client_addrs:
        promises.append(
            run_in_build(f"scp -o 'StrictHostKeyChecking=no' client {c}:/tmp/client")
        )
        promises.append(
            run_in_build(f"scp -o 'StrictHostKeyChecking=no' mfsfuse {c}:/tmp/mfsfuse")
        )
    await asyncio.wait(promises)
    logging.info("Compile Done")


@pytest.fixture(scope="session")
def event_loop():
    return asyncio.get_event_loop()




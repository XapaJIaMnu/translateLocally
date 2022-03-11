#!/usr/bin/env python3
'''A native client simulating the plugin to use for testing the server'''
import asyncio
import itertools
import struct
import json
from pathlib import Path
from pprint import pprint


class Client:
    """asyncio based native messaging client. Main interface is just calling
    `request()` with the right parameters and awaiting the future it returns.
    """
    def __init__(self, *args):
        self.serial = itertools.count(1)
        self.futures = {}
        self.args = args

    async def __aenter__(self):
        self.proc = await asyncio.create_subprocess_exec(*self.args, stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE)
        self.read_task = asyncio.create_task(self.reader())
        return self

    async def __aexit__(self, *args):
        self.read_task.cancel() # cancel?
        self.proc.terminate()

    async def request(self, command, data):
        message_id = next(self.serial)
        message = json.dumps({"command": command, "id": message_id, "data": data}).encode()
        print(f"Sending message {message_id}")
        future = asyncio.get_running_loop().create_future()
        self.futures[message_id] = future
        self.proc.stdin.write(struct.pack("@I", len(message)))
        self.proc.stdin.write(message)
        print(f"Waiting for response on {message_id}")
        return await future

    async def reader(self):
        while True:
            try:
                print("Read loop: start")
                raw_length = await self.proc.stdout.readexactly(4)
                length = struct.unpack("@I", raw_length)[0]
                print(f"Read loop: length received = {length}")
                raw_message = await self.proc.stdout.readexactly(length)
                print(f"Read loop: message received")
                message = json.loads(raw_message)
                
                # Not cool if there is no response message "id" here
                if not "id" in message:
                    continue
                
                # Ignore all progress updates etc for now
                if not "success" in message:
                    continue

                print(f"Received response for {message['id']}")

                future = self.futures[message["id"]]
                if message["success"]:
                    future.set_result(message["data"])
                else:
                    future.set_exception(Exception(message["error"]))
            except asyncio.IncompleteReadError:
                break # Stop read loop if EOF is reached
            except asyncio.CancelledError:
                break # Also stop reading if we're cancelled


class TranslateLocally(Client):
    """TranslateLocally wrapper around Client that translates
    our defined messages into functions with arguments.
    """
    async def list_models(self, include_remote=False):
        return await self.request("ListModels", {"includeRemote": bool(include_remote)})

    async def translate(self, src, trg, text, html=False):
        return await self.request("Translate", {"src": str(src), "trg": str(trg), "text": str(text), "html": bool(html)})

    async def download_model(self, model_id):
        return await self.request("DownloadModel", {"modelID": str(model_id)})


def first(iterable, *default):
    return next(iter(iterable), *default) #passing as rest argument so it can be nothing and trigger StopIteration exception


async def main():
    async with TranslateLocally(Path(__file__).resolve().parent / Path("../build/translateLocally"), "-p") as tl:
        models = await tl.list_models(include_remote=True)
        pprint(models)

        # Models necessary for tests, both direct & pivot
        necessary_models = {("en", "de"), ("en", "es"), ("es", "en")}

        selected_models = {
            (src,trg): first(sorted(
                (
                    model
                    for model in models
                    if src in model["srcTags"] and trg == model["trgTag"]
                ),
                key=lambda model: 0 if model["type"] == "tiny" else 1
            ))
            for src, trg in necessary_models
        }

        pprint(selected_models)

        await asyncio.gather(*(
            tl.download_model(model["id"])
            for model in selected_models.values()
            if not model["local"]))

        translations = await asyncio.gather(
            tl.translate("en", "de", "Hello world!"),
            tl.translate("en", "es", "Sticks and stones may break my bones but words WILL NEVER HURT ME!"),
            tl.translate("es", "de", "¿Por qué no funciona bien?")
        )

        pprint(translations)

    print("Ende")

asyncio.run(main())

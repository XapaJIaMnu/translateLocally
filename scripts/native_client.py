#!/usr/bin/env python3
'''A native client simulating the plugin to use for testing the server'''
import asyncio
import itertools
import struct
import json
import time
import sys
import csv
from pathlib import Path
from pprint import pprint
from tqdm import tqdm


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

    async def request(self, command, data, *, update=lambda data: None):
        message_id = next(self.serial)
        message = json.dumps({"command": command, "id": message_id, "data": data}).encode()
        future = asyncio.get_running_loop().create_future()
        self.futures[message_id] = future, update
        self.proc.stdin.write(struct.pack("@I", len(message)))
        self.proc.stdin.write(message)
        return await future

    async def reader(self):
        while True:
            try:
                raw_length = await self.proc.stdout.readexactly(4)
                length = struct.unpack("@I", raw_length)[0]
                raw_message = await self.proc.stdout.readexactly(length)
                message = json.loads(raw_message)
                
                # Not cool if there is no response message "id" here
                if not "id" in message:
                    continue

                future, update = self.futures[message["id"]]
                
                if "success" in message:
                    if message["success"]:
                        future.set_result(message["data"])
                    else:
                        future.set_exception(Exception(message["error"]))
                elif "update" in message:
                    update(message["data"])
            except asyncio.IncompleteReadError:
                break # Stop read loop if EOF is reached
            except asyncio.CancelledError:
                break # Also stop reading if we're cancelled


class TranslateLocally(Client):
    """TranslateLocally wrapper around Client that translates
    our defined messages into functions with arguments.
    """
    async def list_models(self, *, include_remote=False):
        return await self.request("ListModels", {"includeRemote": bool(include_remote)})

    async def translate(self, src, trg, text, *, html=False):
        result = await self.request("Translate", {"src": str(src), "trg": str(trg), "text": str(text), "html": bool(html)})
        return result["target"]["text"]

    async def download_model(self, model_id, *, update=lambda data: None):
        return await self.request("DownloadModel", {"modelID": str(model_id)}, update=update)


def first(iterable, *default):
    return next(iter(iterable), *default) #passing as rest argument so it can be nothing and trigger StopIteration exception


def get_build():
    return TranslateLocally(Path(__file__).resolve().parent / Path("../build/translateLocally"), "-p")


async def download_with_progress(tl, model, position):
    with tqdm(position=position, desc=model["modelName"], unit="b", unit_scale=True, leave=False) as bar:
        def update(data):
            assert data["read"] <= data["size"]
            bar.total = data["size"]
            diff = data["read"] - bar.n
            bar.update(diff)
        return await tl.download_model(model["id"], update=update)


async def test():
    async with get_build() as tl:
        models = await tl.list_models(include_remote=True)
        pprint(models)

        # Models necessary for tests, both direct & pivot
        necessary_models = {("en", "de"), ("en", "es"), ("es", "en")}

        # From all models available, pick one for every necessary language pair
        # (preferring tiny ones) so we can make sure these are downloaded.
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

        # Download them. Even if they're already model['local'] == True, to test
        # that in that case this is a no-op.
        await asyncio.gather(*(
            download_with_progress(tl, model, position)
            for position, model in enumerate(selected_models.values())
        ))

        # Perform some translations, switching between the models
        translations = await asyncio.gather(
            tl.translate("en", "de", "Hello world!"),
            tl.translate("en", "de", "Let's translate another sentence to German."),
            tl.translate("en", "es", "Sticks and stones may break my bones but words WILL NEVER HURT ME!"),
            tl.translate("en", "de", "I <i>like</i> to drive my car. But I don't have one.", html=True),
            tl.translate("es", "de", "¿Por qué no funciona bien?"),
            tl.translate("en", "de", "This will be the last sentence of the day."),
        )

        pprint(translations)

        assert translations == [
            "Hallo Welt!",
            "Übersetzen wir einen weiteren Satz mit Deutsch.",
            "Palos y piedras pueden romper mis huesos, pero las palabras NUNCA HURT ME.",
            "Ich <i>fahre gerne</i> mein Auto. Aber ich habe keine.", #<i>fahre</i>???
            "Warum funktioniert es nicht gut?",
            "Dies wird der letzte Satz des Tages sein.",
        ]

        # Test bad input
        try:
            await tl.translate("en", "xx", "This is impossible to translate")
            assert False, "How are we able to translate to 'xx'???"
        except Exception as e:
            assert "Failed to load the necessary translation models" in str(e)

        

    print("Fin")


class Timer:
    def __init__(self):
        self.measurements = []

    async def measure(self, coro, *details):
        start = time.perf_counter()
        result = await coro
        end = time.perf_counter()
        self.measurements.append([end - start, *details])
        return result

    def dump(self, fh):
        # TODO stats? For now I just export to Excel or something
        writer = csv.writer(fh)
        writer.writerows(self.measurements)


async def latency_test():
    timer = Timer()

    # Our line generator: just read Crime & Punishment from stdin :D
    lines = (line.strip() for line in sys.stdin)

    async with get_build() as tl:
        for epoch in range(100):
            print(f"Epoch {epoch}...", file=sys.stderr)
            for batch_size in [1, 5, 10, 20, 50, 100]:
                await asyncio.gather(*(
                    timer.measure(
                        tl.translate("en", "de", line),
                        epoch,
                        batch_size,
                        len(line.split(' ')))
                    for n, line in zip(range(batch_size), lines)
                ))

    timer.dump(sys.stdout)


def main():
    if len(sys.argv) == 1:
        print("Usage: {sys.argv[0]} test | latency", file=sys.stderr)
    elif sys.argv[1] == "test":
        asyncio.run(test())
    elif sys.argv[1] == "latency":
        asyncio.run(latency_test())


main()
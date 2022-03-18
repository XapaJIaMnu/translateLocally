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


class Timer:
    """Little helper class top measure runtime of async function calls and dump
    all of those to a CSV.
    """
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
        self.proc.stdin.close()
        await self.proc.wait()

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
                    del self.futures[message["id"]]
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

    async def translate(self, text, src=None, trg=None, *, model=None, pivot=None, html=False):
        if src and trg:
            if model or pivot:
                raise InvalidArgumentException("Cannot combine src + trg and model + pivot arguments")
            spec = {"src": str(src), "trg": str(trg)}
        elif model:
            if pivot:
                spec = {"model": str(model), "pivot": str(pivot)}
            else:
                spec = {"model": str(model)}
        else:
            raise InvalidArgumentException("Missing src + trg or model argument")

        result = await self.request("Translate", {**spec, "text": str(text), "html": bool(html)})
        return result["target"]["text"]

    async def download_model(self, model_id, *, update=lambda data: None):
        return await self.request("DownloadModel", {"modelID": str(model_id)}, update=update)


def first(iterable, *default):
    """Returns the first value of anything iterable, or throws StopIteration
    if it is empty. Or, if you specify a default argument, it will return that.
    """
    return next(iter(iterable), *default) # passing as rest argument so it can be nothing and trigger StopIteration exception


def get_build():
    """Instantiate an asyncio TranslateLocally client that connects to
    tranlateLocally in your local build directory.
    """
    return TranslateLocally(Path(__file__).resolve().parent / Path("../build/translateLocally"), "-p")


async def download_with_progress(tl, model, position):
    """tl.download but with a tqdm powered progress bar."""
    with tqdm(position=position, desc=model["modelName"], unit="b", unit_scale=True, leave=False) as bar:
        def update(data):
            assert data["read"] <= data["size"]
            bar.total = data["size"]
            diff = data["read"] - bar.n
            bar.update(diff)
        return await tl.download_model(model["id"], update=update)


async def test():
    """Test TranslateLocally functionality."""
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
        print() # tqdm messes a lot with the print position, this makes it less bad

        # Test whether the model list has been updated to reflect that the
        # downloaded models are now local.
        models = await tl.list_models(include_remote=True)
        assert all(
            model["local"]
            for selected_model in selected_models.values()
            for model in models
            if model["id"] == selected_model["id"]
        )

        # Perform some translations, switching between the models
        translations = await asyncio.gather(
            tl.translate("Hello world!", "en", "de"),
            tl.translate("Let's translate another sentence to German.", "en", "de"),
            tl.translate("Sticks and stones may break my bones but words WILL NEVER HURT ME!", "en", "es"),
            tl.translate("I <i>like</i> to drive my car. But I don't have one.", "en", "de", html=True),
            tl.translate("¿Por qué no funciona bien?", "es", "de"),
            tl.translate("This will be the last sentence of the day.", "en", "de"),
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
            await tl.translate("This is impossible to translate", "en", "xx")
            assert False, "How are we able to translate to 'xx'???"
        except Exception as e:
            assert "Could not find the necessary translation models" in str(e)

    print("Fin")


async def test_third_party():
    """Test whether TranslateLocally can switch between different types of
    models. This test assumes you have the OPUS repository in your list:
    https://object.pouta.csc.fi/OPUS-MT-models/app/models.json
    """
    async with get_build() as tl:
        models_to_try = [
            'en-de-tiny',
            'en-de-base',
            'eng-fin-tiny', # model has broken model_info.json so won't work anyway :(
            'eng-ukr-tiny',
        ]

        models = await tl.list_models(include_remote=True)

        # Select a model from the model list for each of models_to_try, but
        # leave it out if there is no model available.
        selected_models = {
            shortname: model
            for shortname in models_to_try
            if (model := first((model for model in models if model["shortname"] == shortname), None))
        }

        await asyncio.gather(*(
            download_with_progress(tl, model, position)
            for position, model in enumerate(selected_models.values())
        ))

        # TODO: Temporary filter to figure out 'failed' downloads. eng-fin-tiny
        # has a broken JSON file so it will download correctly, but still not
        # be available or show up in this list. We should probably make the
        # download fail in that scenario.
        models = await tl.list_models(include_remote=False)
        for shortname in list(selected_models.keys()):
            if not any(True for model in models if model["shortname"] == shortname):
                print(f"Skipping {shortname} because it didn't show up in model list after downloading", file=sys.stderr)
                del selected_models[shortname]
        
        translations = await asyncio.gather(*[
            tl.translate("This is a very simple test sentence", model=model["id"])
            for model in selected_models.values()
        ])

        pprint(list(zip(selected_models.keys(), translations)))


async def test_latency():
    timer = Timer()

    # Our line generator: just read Crime & Punishment from stdin :D
    lines = (line.strip() for line in sys.stdin)

    async with get_build() as tl:
        for epoch in range(100):
            print(f"Epoch {epoch}...", file=sys.stderr)
            for batch_size in [1, 5, 10, 20, 50, 100]:
                await asyncio.gather(*(
                    timer.measure(
                        tl.translate(line, "en", "de"),
                        epoch,
                        batch_size,
                        len(line.split(' ')))
                    for n, line in zip(range(batch_size), lines)
                ))

    timer.dump(sys.stdout)


def main():
    tests = {
        "test": test,
        "third-party": test_third_party,
        "latency": test_latency,
    }

    if len(sys.argv) == 1 or sys.argv[1] not in tests:
        print("Usage: {sys.argv[0]} test | latency", file=sys.stderr)
    else:
        asyncio.run(tests[sys.argv[1]]())


main()
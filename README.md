# translateLocally
Fast and secure translation on your local machine with a GUI, powered by marian and Bergamot.

Bringing fast and secure machine translation to the masses! To build and run
```bash
mkdir build
cd build
cmake ..
make -j5
./translateLocally
```

Requires `QT>=5 libarchive intel-mkl-static`. We make use of the `QT>=5 network`, `QT>=5 linguisticTool` and `QT>=5 svg` components. Depending on your distro, those may be split in separate package from your QT package (Eg `qt{6/7}-tools-dev`; `qt{5/6}-svg` or `libqt5svg5-dev`). QT6 is fully supported and its use is encouraged. `intel-mkl-static` may be part of `mkl` or `intel-mkl` packages.

## Command line interface
translateLocally supports using the command line to perform translations. Example usage:
```bash
./translateLocally --help
Usage: ./translateLocally [options]
A secure translation service that performs translations for you locally, on your own machine.

Options:
  -h, --help                     Displays help on commandline options.
  --help-all                     Displays help including Qt specific options.
  -v, --version                  Displays version information.
  -l, --list-models              List locally installed models.
  -a, --available-models         Connect to the Internet and list available
                                 models. Only shows models that are NOT
                                 installed locally or have a new version
                                 available online.
  -d, --download-model <output>  Connect to the Internet and download a model.
  -r, --remove-model <output>    Remove a model from the local machine. Only
                                 works for models managed with translateLocally.
  -m, --model <model>            Select model for translation.
  -i, --input <input>            Source translation text (or just used stdin).
  -o, --output <output>          Target translation output (or just used
                                 stdout).
```

### Downloading models from CLI
Models can be downloaded from the GUI or the CLI. For the CLI model management you need to:
```bash
$ ./translateLocally -a
Czech-English type: base version: 1; To download do -d cs-en-base
German-English type: base version: 2; To download do -d de-en-base
English-Czech type: base version: 1; To download do -d en-cs-base
English-German type: base version: 2; To download do -d en-de-base
English-Estonian type: tiny version: 1; To download do -d en-et-tiny
Estonian-English type: tiny version: 1; To download do -d et-en-tiny
Icelandic-English type: tiny version: 1; To download do -d is-en-tiny
Norwegian (Bokmal)-English type: tiny version: 1; To download do -d nb-en-tiny
Norwegian (Nynorsk)-English type: tiny version: 1; To download do -d nn-en-tiny

$ ./translateLocally -d en-et-tiny
Downloading English-Estonian type: tiny...
100% [############################################################]
Model downloaded succesffully! You can now invoke it with -m en-et-tiny
```

### Removing models from the CLI
Models can be removed from the GUI or the CLI. For the CLI model removal, you need to:
```bash
./translateLocally -r en-et-tiny
Model English-Estonian type tiny successfully removed.
```

### Listing available models
The avialble models can be listed with `-l`
```bash
./translateLocally -l
Czech-English type: tiny version: 1; To invoke do -m cs-en-tiny
German-English type: tiny version: 2; To invoke do -m de-en-tiny
English-Czech type: tiny version: 1; To invoke do -m en-cs-tiny
English-German type: tiny version: 2; To invoke do -m en-de-tiny
English-Spanish type: tiny version: 1; To invoke do -m en-es-tiny
Spanish-English type: tiny version: 1; To invoke do -m es-en-tiny
```

### Translating a single sentence
Note that customising the translator settings can only be done via the GUI.
```bash
echo "Me gustaria comprar la casa verde" | ./translateLocally -m es-en-tiny
```

### Translating a whole dataset
```bash
sacrebleu -t wmt13 -l en-es --echo ref > /tmp/es.in
./translateLocally -m es-en-tiny -i /tmp/es.in -o /tmp/en.out
```

### Pivoting and piping
The command line interface can be used to chain several translation models to achieve pivot translation, for example Spanish to German.
```bash
sacrebleu -t wmt13 -l en-es --echo ref > /tmp/es.in
cat /tmp/es.in | ./translateLocally -m es-en-tiny | ./translateLocally -m en-de-tiny -o /tmp/de.out
```

### Using in an environment without a running display server
translateLocally is built on top of QT, with modules linked in such a way that a display server required in order for the program to start, even if we only use the command line interface, resulting in an error like:
```bash
$ ./translateLocally --version
loaded library "/opt/conda/plugins/platforms/libqxcb.so"
qt.qpa.xcb: could not connect to display
qt.qpa.plugin: Could not load the Qt platform plugin "xcb" in "" even though it was found.
This application failed to start because no Qt platform plugin could be initialized. Reinstalling the application may fix this problem.

Available platform plugins are: eglfs, minimal, minimalegl, offscreen, vnc, webgl, xcb.

Aborted
```
To get around this, we can use set `QT_QPA_PLATFORM=offscreen` if we have the `offscreen` plugin:
```bash
QT_QPA_PLATFORM=offscreen ./translateLocally --version
translateLocally v0.0.2+a603422
```
OR use `xvfb` to emulate a server.
```bash
xvfb-run --auto-servernum ./translateLocally --version
translateLocally v0.0.2+a603422
```
Note that this issue only occurs on Linux, as Windows and Mac (at least to my knowledge) always have an active display even in remote sessions.

# Acnowledgements
<img src="https://raw.githubusercontent.com/XapaJIaMnu/translateLocally/master/eu-logo.png" data-canonical-src="https://raw.githubusercontent.com/XapaJIaMnu/translateLocally/master/eu-logo.png" width=10% />

This project has received funding from the European Union’s Horizon 2020 research and innovation programme under grant agreement No 825303.

## Bergamot
This project was made possible through the combined effort of all researchers and partners in the Bergamot project https://browser.mt/partners/ . The translation models are prepared as part of the Bergamot project https://github.com/browsermt/students . The translation engine used is https://github.com/browsermt/bergamot-translator which is based on marian https://github.com/marian-nmt/marian-dev .

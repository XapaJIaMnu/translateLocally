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
  -h, --help             Displays help on commandline options.
  --help-all             Displays help including Qt specific options.
  -v, --version          Displays version information.
  -l, --list-models      List available models.
  -m, --model <model>    Select model for translation.
  -i, --input <input>    Source translation text (or just used stdin).
  -o, --output <output>  Target translation output (or just used stdout).
```

### Listing available models
The user needs to download the model and customise the translator settings from the GUI. Then, the avialble models can be listed with `-l`
```
./translateLocally -l
Czech-English type: tiny version: 1; To invoke do -m cs-en-tiny
German-English type: tiny version: 2; To invoke do -m de-en-tiny
English-Czech type: tiny version: 1; To invoke do -m en-cs-tiny
English-German type: tiny version: 2; To invoke do -m en-de-tiny
English-Spanish type: tiny version: 1; To invoke do -m en-es-tiny
Spanish-English type: tiny version: 1; To invoke do -m es-en-tiny
```

### Translating a single sentence
```
echo "Me gustaria comprar la casa verde" | ./translateLocally -m es-en-tiny
```

### Translating a whole dataset
```
sacrebleu -t wmt13 -l en-es --echo ref > /tmp/es.in
./translateLocally -m es-en-tiny -i /tmp/es.in -o /tmp/en.out
```

### Pivoting and piping
The command line interface can be used to chain several translation models to achieve pivot translation, for example Spanish to German.
```
sacrebleu -t wmt13 -l en-es --echo ref > /tmp/es.in
cat /tmp/es.in | ./translateLocally -m es-en-tiny | ./translateLocally -m en-de-tiny -o /tmp/de.out
```


# Acnowledgements
<img src="https://raw.githubusercontent.com/XapaJIaMnu/translateLocally/master/eu-logo.png" data-canonical-src="https://raw.githubusercontent.com/XapaJIaMnu/translateLocally/master/eu-logo.png" width=10% />

This project has received funding from the European Union’s Horizon 2020 research and innovation programme under grant agreement No 825303.

## Bergamot
This project was made possible through the combined effort of all researchers and partners in the Bergamot project https://browser.mt/partners/ . The translation models are prepared as part of the Bergamot project https://github.com/browsermt/students . The translation engine used is https://github.com/browsermt/bergamot-translator which is based on marian https://github.com/marian-nmt/marian-dev .

# translateLocally
Fast and secure translation on your local machine with a GUI, powered by marian and Bergamot.

## Downloads

You can download the latest automatic build for Windows, Linux and Mac from the [releases](https://github.com/XapaJIaMnu/translateLocally/releases) github tab or from the [official website](https://translatelocally.com).


## Compile from source
Bringing fast and secure machine translation to the masses! To build and run
```bash
mkdir build
cd build
cmake ..
make -j5
./translateLocally
```

Requires `QT>=5 libarchive intel-mkl-static`. We make use of the `QT>=5 network`, `QT>=5 linguisticTool` and `QT>=5 svg` components. Depending on your distro, those may be split in separate package from your QT package (Eg `qt{6/7}-tools-dev`; `qt{5/6}-svg` or `libqt5svg5-dev`). QT6 is fully supported and its use is encouraged. `intel-mkl-static` may be part of `mkl` or `intel-mkl` packages.

## MacOS Build
On MacOS, translateLocally doesn't rely on MKL, but instead on Apple accelerate. If you want to build a portable executable that is able to run on multiple machines, we recommend using Qt's distribution of Qt, as opposed to homebrew's due to issues with [macdeployqt](https://github.com/XapaJIaMnu/translateLocally/issues/69). To produce a `.dmg`do:
```bash
mkdir build
cd build
cmake ..
cmake --build . -j3 --target translateLocally-bin translateLocally.dmg
```
Alternatively, if you wish to sign and notarize the `.dmg`for distribution, you may use [macdmg.sh](dist/macdmg.sh)
```bash
mkdir build
cd build
cmake ..
make -j5
../dist/macdmg.sh .
```
Check the script for the environment variables that you need to set if you want to take advantage of signing and notarization.

## Windows Build
On Windows, we recommend using [vcpkg](https://github.com/Microsoft/vcpkg) to install all necessary packages and Visual Studio to perform the build.

# Command line interface
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

## Downloading models from CLI
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

## Removing models from the CLI
Models can be removed from the GUI or the CLI. For the CLI model removal, you need to:
```bash
./translateLocally -r en-et-tiny
Model English-Estonian type tiny successfully removed.
```

## Listing available models
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

## Translating a single sentence
Note that customising the translator settings can only be done via the GUI.
```bash
echo "Me gustaria comprar la casa verde" | ./translateLocally -m es-en-tiny
```

## Translating a whole dataset
```bash
sacrebleu -t wmt13 -l en-es --echo ref > /tmp/es.in
./translateLocally -m es-en-tiny -i /tmp/es.in -o /tmp/en.out
```

Note that if you are using the macOS translateLocally.app version, the `-i` and `-o` options are not able to read most files. You can use pipes instead, e.g.
```bash
translateLocally.app/Contents/MacOS/translateLocally -m es-en-tiny < input.txt > output.txt
```

## Pivoting and piping
The command line interface can be used to chain several translation models to achieve pivot translation, for example Spanish to German.
```bash
sacrebleu -t wmt13 -l en-es --echo ref > /tmp/es.in
cat /tmp/es.in | ./translateLocally -m es-en-tiny | ./translateLocally -m en-de-tiny -o /tmp/de.out
```

# Importing custom models
translateLocally supports importing custom models. translateLocally uses the [Bergamot](https://github.com/browsermt/marian-dev) fork of [marian](https://github.com/marian-nmt/marian-dev). As such, it supports the vast majority marian models out of the box. You can just train your marian model and place it a directory. 
## Basic model import
The directory structure of a translateLocally model looks like this:
```bash
$ tree my-custom-model
my-custom-model/
├── config.intgemm8bitalpha.yml
├── model_info.json
├── model.npz
└── vocab.deen.spm
```
The `config.intgemm8bitalpha.yml` name is hardcoded, and so is `model_info.json`. Everything else could have an arbitrary name. translateLocally will load the model according to the settings specified in `config.intgemm8bitalpha.yml`. These are just normal marian configuration options. `model_info.json` contains metadata about the model:
```bash
$ cat model_info.json 
{
  "modelName": "German-English tiny",
  "shortName": "de-en-tiny",
  "type":      "tiny",
  "src":       "German",
  "trg":       "English",
  "version":   2.0,
  "API":       1.0
}
```
Once the files are in place, tar the model:
```bash
$ tar -czvf my-custom-model.tar.gz my-custom-model
```
And you can import it via the GUI: Open translateLocally and go to **Edit -> Translator Settings -> Languages -> Import model** and navigate to the archive you created. 

## Quantising the model
The process described above will create a model usable by translateLocally, albeit not a very efficient one. In order to create an efficient model we recommend that you quantise the model to 8-bit integers. You can do that by downloading and compiling the [Bergamot](https://github.com/browsermt/marian-dev) fork of marian, and using `marian-conv` to create the quantised model:
```bash
$MARIAN/marian-conv -f input_model.npz -t output_model.bin --gemm-type intgemm8
```
And then changing your configuration `config.intgemm8bitalpha.yml` to point to this new model, as well as appending `gemm-precision: int8shift` to it.

## Further increasing performance
**For best results, we strongly recommend that you use student models.** Instructions on how to create one + scripts can be found [here](https://github.com/browsermt/students/tree/master/train-student) and a detailed video tutorial and explanations are available [here](https://nbogoychev.com/efficient-machine-translation/). Student models are typically at least 8X faster than teacher models such as the transformer-base preset.

You can further achive another 30\%-40\% performance boost if you precompute the quantisation multipliers of the model and you use a lexical shortlist. The process for those is described in details at the Bergamot project's [Github](https://github.com/browsermt/students/tree/master/train-student#5-8-bit-quantization). Remember that you need to use the [Bergamot](https://github.com/browsermt/marian-dev) fork of Marian.

Example script that converts a marian model to the most efficient 8-bit representation can also be found at Bergamot's [Github](https://github.com/browsermt/students/blob/master/esen/esen.student.tiny11/speed.cpu.intgemm8bitalpha.sh).
# Acknowledgements
<img src="https://raw.githubusercontent.com/XapaJIaMnu/translateLocally/master/eu-logo.png" data-canonical-src="https://raw.githubusercontent.com/XapaJIaMnu/translateLocally/master/eu-logo.png" width=10% />

This project has received funding from the European Union’s Horizon 2020 research and innovation programme under grant agreement No 825303.

## Bergamot
This project was made possible through the combined effort of all researchers and partners in the Bergamot project https://browser.mt/partners/ . The translation models are prepared as part of the Bergamot project https://github.com/browsermt/students . The translation engine used is https://github.com/browsermt/bergamot-translator which is based on marian https://github.com/marian-nmt/marian-dev .

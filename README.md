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

Requires `QT>=5.15 libarchive`. We make use of the `QT>=5.15 network` and `QT>=5.15 linguisticTool` components. Depending on your distro, those may be split in separate package from your QT package. QT6 is fully supported and its use is encouraged.

# Acnowledgements
<img src="https://raw.githubusercontent.com/XapaJIaMnu/translateLocally/master/eu-logo.png" data-canonical-src="https://raw.githubusercontent.com/XapaJIaMnu/translateLocally/master/eu-logo.png" width=10% />

This project has received funding from the European Union’s Horizon 2020 research and innovation programme under grant agreement No 825303.

## Bergamot
This project was made possible through the combined effort of all researchers and partners in the Bergamot project https://browser.mt/partners/ . The translation models are prepared as part of the Bergamot project https://github.com/browsermt/students . The translation engine used is https://github.com/browsermt/bergamot-translator which is based on marian https://github.com/marian-nmt/marian-dev .

#include "NativeMsgIface.h"
#include <memory>

NativeMsgIface::NativeMsgIface(QObject * parent) :
      QObject(parent)
      , eventLoop_(this)
      , network_(this)
      , settings_(this)
      , models_(this, &settings_)
      , translator_(new MarianInterface(this))
      , die_(false) {
    connect(translator_, &MarianInterface::error, this, &NativeMsgIface::outputError);
    connect(translator_, &MarianInterface::translationReady, this, &NativeMsgIface::outputTranslation);

    // For testing purposes load the first available model on the system. In the future we will have json communication that instructs what models to load
    std::cerr << "Loading model: " << models_.getInstalledModels().first().modelName.toStdString() << std::endl;
    translator_->setModel(models_.getInstalledModels().first().path, settings_.marianSettings());
}

int NativeMsgIface::run() {
    die_ = false; // Do not die immediately
    std::cerr << "Native msg interface accepting messages..." << std::endl;
    do {
        // First part of the message: Size of the input
        char len[4];
        std::cin.read(len, 4);
        unsigned int ilen = *reinterpret_cast<unsigned int *>(len);
        if (ilen < kMaxInputLength && ilen>1) {
            std::cerr << "Received message size of: " << ilen << std::endl;
            // This will be a json which is parsed and decoded, but for now just translate
            std::unique_ptr<char[]> input(new char[ilen]);
            std::cin.read(input.get(), ilen);
            std::cerr << "Received message size of: " << ilen << " with content: " << input.get() << std::endl;
            translator_->translate(QString(input.get()));
            eventLoop_.exec(); // Do not read any input before this translation is done
        } else {
          // @TODO Consume any invalid input here
          QThread::msleep(1000); // don't busy wait. @TODO those should be be done with Qevent
        }
    } while (!die_);
    std::cerr << "Native msg interface no longer accepting messages..." << std::endl;
    return 0;
}

void NativeMsgIface::die() {
    die_ = true;
}

void NativeMsgIface::outputError(QString err) {
    std::cout << err.toStdString() << std::endl;
    if (eventLoop_.isRunning()) {
        eventLoop_.exit();
    }
}

void NativeMsgIface::outputTranslation(Translation output) {
    std::cout << output.translation().toStdString() << std::endl;
    eventLoop_.exit();
}

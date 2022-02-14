#include "NativeMsgIface.h"
#include <memory>
#include <QThread>

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

    inputWorker_ = std::thread([&](){
        do {
            if ((std::cin.peek() == std::char_traits<char>::eof())) {
                std::cerr << "Reached EOF CIN" << std::endl;
                // Send a final package telling the consumer to die
                die_ = true;
                TranslationRequest poison;
                poison.die = true;
                translationQueue_.enqueue(poison);
                break;
            }
            // First part of the message: Size of the input
            char len[4];
            std::cin.read(len, 4);
            unsigned int ilen = *reinterpret_cast<unsigned int *>(len);
            if (ilen < kMaxInputLength && ilen>1) {
                //std::cerr << "Received message size of: " << ilen << std::endl;
                // This will be a json which is parsed and decoded, but for now just translate
                std::unique_ptr<char[]> input(new char[ilen]);
                std::cin.read(input.get(), ilen);
                TranslationRequest myJsonInput = parseJsonInput(input.get(), ilen);
                //std::cerr << "Received message size of: " << ilen << " with content: " << myJsonInput.text.toStdString() << std::endl;
                die_ = myJsonInput.die;
                translationQueue_.enqueue(myJsonInput);
            } else {
              // @TODO Consume any invalid input here
              std::cerr << "Unknown input, aboring for now. Will handle gracefully later" << std::endl;
              std::abort();
            }
        } while (!die_);
    });

    outputWorker_ = std::thread([&](){
       do {
            TranslationRequest request = translationQueue_.dequeue();
            std::cerr << "Consumed some " << request.text.toStdString() << " should I die " << std::boolalpha << request.die << std::endl;
            if (request.die) { // Die condition
                std::cerr << "Dying" << std::endl;
                break;
            }
            translator_->translate(request.text); // Does not work. We need a blocking API here
        } while (true);
    });
}

int NativeMsgIface::run() {/*
    die_ = false; // Do not die immediately
    std::cerr << "Native msg interface accepting messages..." << std::endl;
    do {
        if ((std::cin.peek() == std::char_traits<char>::eof())) {
            QThread::msleep(1000);
            std::cerr << "Reached EOF CIN" << std::endl;
            continue;
        }
        QThread::msleep(700);
        // First part of the message: Size of the input
        char len[4];
        std::cin.read(len, 4);
        unsigned int ilen = *reinterpret_cast<unsigned int *>(len);
        if (ilen < kMaxInputLength && ilen>1) {
            std::cerr << "Received message size of: " << ilen << std::endl;
            // This will be a json which is parsed and decoded, but for now just translate
            std::unique_ptr<char[]> input(new char[ilen]);
            std::cin.read(input.get(), ilen);
            TranslationRequest myJsonInput = parseJsonInput(input.get(), ilen);
            std::cerr << "Received message size of: " << ilen << " with content: " << myJsonInput.text.toStdString() << std::endl;
            translator_->translate(myJsonInput.text);
            eventLoop_.exec(); // Do not read any input before this translation is done
        } else {
          // @TODO Consume any invalid input here
          QThread::msleep(1000); // don't busy wait. @TODO those should be be done with Qevent
        }
    } while (!die_);
    std::cerr << "Native msg interface no longer accepting messages..." << std::endl;*/
    //QThread::sleep(4);
    while (!die_) {
        QThread::sleep(1);
    }
    inputWorker_.join();
    outputWorker_.join();
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

TranslationRequest NativeMsgIface::parseJsonInput(char * bytes, size_t length) {
    QByteArray inputBytes(bytes, length);
    QJsonDocument inputJson = QJsonDocument::fromJson(inputBytes);
    QJsonObject jsonObj = inputJson.object();
    TranslationRequest ret;
    ret.text = jsonObj[QString("text")].toString();
    ret.die = jsonObj[QString("die")].toBool();
    return ret;
}

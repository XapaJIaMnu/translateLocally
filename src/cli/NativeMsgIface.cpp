#include "NativeMsgIface.h"
#include <QThread>

// bergamot-translator
#include "3rd_party/bergamot-translator/src/translator/service.h"
#include "3rd_party/bergamot-translator/src/translator/parser.h"
#include "3rd_party/bergamot-translator/src/translator/response.h"

namespace  {

std::shared_ptr<marian::Options> makeOptions(const std::string &path_to_model_dir, const translateLocally::marianSettings &settings) {
    std::shared_ptr<marian::Options> options(marian::bergamot::parseOptionsFromFilePath(path_to_model_dir + "/config.intgemm8bitalpha.yml"));
    options->set("cpu-threads", settings.cpu_threads,
                 "workspace", settings.workspace,
                 "mini-batch-words", 1000,
                 "alignment", "soft",
                 "quiet", true);
    return options;
}

}

NativeMsgIface::NativeMsgIface(QObject * parent) :
      QObject(parent)
      , network_(this)
      , settings_(this)
      , models_(this, &settings_)
      , die_(false) {
    // Create service
    marian::bergamot::AsyncService::Config serviceConfig;
    serviceConfig.numWorkers = settings_.marianSettings().cpu_threads;
    serviceConfig.cacheEnabled = settings_.marianSettings().translation_cache;
    serviceConfig.cacheSize = kTranslationCacheSize;
    serviceConfig.cacheMutexBuckets = settings_.marianSettings().cpu_threads;

    // Free up old service first (see https://github.com/browsermt/bergamot-translator/issues/290)
    // Calling clear to remove any pending translations so we
    // do not have to wait for those when AsyncService is destroyed.
    service_.reset();

    service_ = std::make_shared<marian::bergamot::AsyncService>(serviceConfig);
    // Need to make sure that no translations are in flight while changing the model.
    auto modelConfig = makeOptions(models_.getInstalledModels().first().path.toStdString(), settings_.marianSettings());
    model_ = std::make_shared<marian::bergamot::TranslationModel>(modelConfig, settings_.marianSettings().cpu_threads);

    // For testing purposes load the first available model on the system. In the future we will have json communication that instructs what models to load
    std::cerr << "Loading model: " << models_.getInstalledModels().first().modelName.toStdString() << std::endl;

    inputWorker_ = std::thread([&](){
        do {
            if ((std::cin.peek() == std::char_traits<char>::eof())) {
                std::cerr << "Reached EOF CIN" << std::endl;
                // Send a final package telling the consumer to die
                QThread::sleep(1);
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
                int myID = myJsonInput.id;
                std::cerr << "Received message size of: " << ilen << " with content: " << myJsonInput.text.toStdString() << std::endl;
                //die_ = myJsonInput.die;
                service_->translate(model_, std::move(myJsonInput.text.toStdString()), [&, myID] (marian::bergamot::Response&& val) {
                   QByteArray outputBytesJson = toJsonBytes(std::move(val), myID);
                   std::lock_guard<std::mutex> lock(coutmutex_);
                   std::cout << outputBytesJson.data() << std::endl;
                });
            } else {
              // @TODO Consume any invalid input here
              std::cerr << "Unknown input, aboring for now. Will handle gracefully later" << std::endl;
              std::abort();
            }
        } while (!die_);
    });

}

int NativeMsgIface::run() {
    //while (!die_) {
        QThread::sleep(4);
    //}
    inputWorker_.join();
    return 0;
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

inline QByteArray NativeMsgIface::toJsonBytes(marian::bergamot::Response&& response, int myID) {
    QJsonObject jsonObj;
    jsonObj.insert(QString("id"), myID);
    jsonObj.insert(QString("text"), QString::fromStdString(response.target.text));
    QByteArray bytes = QJsonDocument(jsonObj).toJson();
    return bytes;
}

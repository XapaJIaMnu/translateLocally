#include "NativeMsgIface.h"
#include <QThread>
#include <QJsonArray>

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
    // Disable synchronisation with C style streams. That should make IO faster
    std::ios_base::sync_with_stdio(false);

    // Need to make sure that no translations are in flight while changing the model.
    auto modelConfig = makeOptions(models_.getInstalledModels().first().path.toStdString(), settings_.marianSettings());
    model_ = std::make_shared<marian::bergamot::TranslationModel>(modelConfig, settings_.marianSettings().cpu_threads);

    // For testing purposes load the first available model on the system. In the future we will have json communication that instructs what models to load
    std::cerr << "Loading model: " << models_.getInstalledModels().first().modelName.toStdString() << std::endl;

    inputWorker_ = std::thread([&](){
        // Create service
        marian::bergamot::AsyncService::Config serviceConfig;
        serviceConfig.numWorkers = settings_.marianSettings().cpu_threads;
        serviceConfig.cacheEnabled = settings_.marianSettings().translation_cache;
        serviceConfig.cacheSize = kTranslationCacheSize;
        serviceConfig.cacheMutexBuckets = settings_.marianSettings().cpu_threads;
        std::unique_ptr<marian::bergamot::AsyncService> service = std::make_unique<marian::bergamot::AsyncService>(serviceConfig);
        do {
            if ((std::cin.peek() == std::char_traits<char>::eof())) {
                // Send a final package telling other stuff to die
                die_ = true;
                break;
            }
            // First part of the message: Size of the input
            char len[4];
            std::cin.read(len, 4);
            unsigned int ilen = *reinterpret_cast<unsigned int *>(len);
            if (ilen < kMaxInputLength && ilen>1) {
                // This will be a json which is parsed and decoded, but for now just translate
                std::unique_ptr<char[]> input(new char[ilen]);
                std::cin.read(input.get(), ilen);
                TranslationRequest myJsonInput = parseJsonInput(input.get(), ilen);
                marian::bergamot::ResponseOptions options;
                int myID = myJsonInput.id;
                //std::cerr << "Received message size of: " << ilen << " with content: " << myJsonInput.text.toStdString() << std::endl;
                //die_ = myJsonInput.die;
                service->translate(model_, std::move(myJsonInput.text.toStdString()), [&, myID] (marian::bergamot::Response&& val) {
                    QByteArray outputBytesJson = toJsonBytes(std::move(val), myID);
                    std::lock_guard<std::mutex> lock(coutmutex_);

                    size_t outputSize = outputBytesJson.size();
                    std::cerr << "Writing response: " << outputSize << " " << outputBytesJson.data() << std::endl;
                    std::cout.write(reinterpret_cast<char*>(&outputSize), 4);
                    std::cout.write(outputBytesJson.data(), outputSize);
                    std::cout.flush();
                }, options);
            } else {
              // @TODO Consume any invalid input here
              std::cerr << "Unknown input, aboring for now. Will handle gracefully later" << std::endl;
              std::abort();
            }
        } while (!die_);
    });

}

int NativeMsgIface::run() {
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
    ret.id = jsonObj[QString("id")].toInt();
    return ret;
}

inline QByteArray NativeMsgIface::toJsonBytes(marian::bergamot::Response&& response, int myID) {
    QJsonObject jsonObj;
    jsonObj.insert(QString("id"), myID);
    QJsonObject text;
    text.insert(QString("text"), QString::fromStdString(response.target.text));
    jsonObj.insert(QString("target"), text);
    QByteArray bytes = QJsonDocument(jsonObj).toJson();
    return bytes;
}

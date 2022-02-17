#include "NativeMsgIface.h"
#include <QJsonDocument>

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
      , die_(false)
      , eventLoop_(this)
    {
    // Connections
    connect(&models_, &ModelManager::fetchedRemoteModels, this, [&](){eventLoop_.exit();});
    connect(&network_, &Network::downloadComplete, this, [&](QFile *file, QString filename) {
        models_.writeModel(file, filename);
        eventLoop_.exit();
    });
    modelMapInit(models_.getInstalledModels());

    // Disable synchronisation with C style streams. That should make IO faster
    std::ios_base::sync_with_stdio(false);

    inputWorker_ = std::thread([&](){
        // Create service
        marian::bergamot::AsyncService::Config serviceConfig;
        serviceConfig.numWorkers = settings_.marianSettings().cpu_threads;
        serviceConfig.cacheEnabled = settings_.marianSettings().translation_cache;
        serviceConfig.cacheSize = kTranslationCacheSize;
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
                options.HTML = myJsonInput.html;
                int myID = myJsonInput.id;
                std::cerr << "Received message size of: " << ilen << " with content: " << myJsonInput.text.toStdString() << std::endl;
                bool success = tryLoadModel(myJsonInput.src, myJsonInput.trg);
                std::cerr << "Loaded model: " << model_.first.first.toStdString() << "-" << model_.first.second.toStdString() << " " << std::boolalpha << success << std::endl;
                std::function<void(marian::bergamot::Response&&)> callbackLambda = [&, myID](marian::bergamot::Response&& val) {
                    QByteArray outputBytesJson = toJsonBytes(std::move(val), myID);
                    std::lock_guard<std::mutex> lock(coutmutex_);

                    size_t outputSize = outputBytesJson.size();
                    std::cerr << "Writing response: " << outputSize << " " << outputBytesJson.data() << std::endl;
                    std::cout.write(reinterpret_cast<char*>(&outputSize), 4);
                    std::cout.write(outputBytesJson.data(), outputSize);
                    std::cout.flush();};
                if (!success) {
                    QByteArray errOutput = errJson(myID, QString("Failed to load the necessary translation models."));
                    std::lock_guard<std::mutex> lock(coutmutex_);
                    size_t outputSize = errOutput.size();
                    std::cout.write(reinterpret_cast<char*>(&outputSize), 4);
                    std::cout.write(errOutput.data(), outputSize);
                    std::cout.flush();
                    std::cerr << "Here" << std::endl;
                } else if (pivotModel_.first.first == QString("none")) {
                    std::cerr << "elif " << model_.first.first.toStdString() << "-" << model_.first.second.toStdString() << std::endl;
                    service->translate(model_.second, std::move(myJsonInput.text.toStdString()), callbackLambda, options);
                } else {
                    std::cerr << "zelf" << " " << pivotModel_.first.first.toStdString() << std::endl;
                    service->pivot(model_.second, pivotModel_.second, std::move(myJsonInput.text.toStdString()), callbackLambda, options);
                }
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
    ret.html = jsonObj[QString("html")].toBool();
    ret.src = jsonObj[QString("src")].toString();
    ret.trg = jsonObj[QString("trg")].toString();
    return ret;
}

inline QByteArray NativeMsgIface::errJson(int myID, QString err) {
    QJsonObject jsonObj;
    jsonObj.insert(QString("id"), myID);
    QJsonObject text;
    text.insert(QString("text"), err);
    jsonObj.insert(QString("target"), text);
    jsonObj.insert(QString("success"), false);
    QByteArray bytes = QJsonDocument(jsonObj).toJson();
    return bytes;
}

inline QByteArray NativeMsgIface::toJsonBytes(marian::bergamot::Response&& response, int myID) {
    QJsonObject jsonObj;
    jsonObj.insert(QString("id"), myID);
    QJsonObject text;
    text.insert(QString("text"), QString::fromStdString(response.target.text));
    jsonObj.insert(QString("target"), text);
    jsonObj.insert(QString("success"), true);
    QByteArray bytes = QJsonDocument(jsonObj).toJson();
    return bytes;
}

bool NativeMsgIface::tryLoadModel(QString srctag, QString trgtag) {
    // First, check if we have everything required already loaded:
    if ((model_.first.first == srctag && model_.first.second == trgtag) ||
            ((model_.first.first == srctag && model_.first.second == QString("en") && pivotModel_.first.first == QString("en") && pivotModel_.first.second == trgtag))) {
        return true;
    }
    // Go through all local models and check if we can do the requested language pair
    QPair<bool, Model> candidate = findModelHelper(srctag, trgtag);
    QPair<bool, Model> pivotCandidate({false, Model()});
    bool pivotRequired = false;

    if (!candidate.first) { // We didn't find the model among the local models, try fetching remote models and do it again.
        if (models_.getNewModels().isEmpty()) {
            models_.fetchRemoteModels();
            modelMapInit(models_.getNewModels());
            candidate = findModelHelper(srctag, trgtag);
        }
    }

    if (!candidate.first) {
        // We STILL didn't find a model. Try pivoting now.
        pivotRequired = true;
        // @TODO find ANY possible pivot language combination, but for now, just assume the bridging language is English
        candidate = findModelHelper(srctag, QString("en"));
        pivotCandidate = findModelHelper(QString("en"), trgtag);
    }
    // Load what we have found, if we have found it
    if (candidate.first) {
        if (candidate.second.path.isEmpty()) {
            return false; //@TODO remove when model fetching is implemented
        }
        auto modelConfig = makeOptions(candidate.second.path.toStdString(), settings_.marianSettings());
        model_ = {{srctag, candidate.second.trgTag}, std::make_shared<marian::bergamot::TranslationModel>(modelConfig, settings_.marianSettings().cpu_threads)};
        if (pivotCandidate.first && candidate.second.path.isEmpty()) {
            return false; //@TODO remove when model fetching is implemented
        }
        if (pivotCandidate.first) {
            auto modelPivotConfig = makeOptions(pivotCandidate.second.path.toStdString(), settings_.marianSettings());
            pivotModel_ = {{QString("en"), trgtag}, std::make_shared<marian::bergamot::TranslationModel>(modelPivotConfig, settings_.marianSettings().cpu_threads)};
        }
    }
    if (!pivotRequired && candidate.first) {
        // Clear the pivot model
        pivotModel_.first = {QString("none"), QString("none")};
        return true;
    } else if (candidate.first && pivotCandidate.first) {
        return true;
    } else {
        return false;
    }
}

//@TODO Move this to the model Manager class and abolish the old list
void NativeMsgIface::modelMapInit(QList<Model> myModelList) {
    for (auto&& model : myModelList) {
        QStringList srcTags;
        for (auto&& tag : model.srcTags.keys()) {
            srcTags.append(tag);
            if (modelMap_.find(tag) != modelMap_.end()) {
                if (modelMap_[tag].find(model.trgTag) != modelMap_[tag].end()) {
                    modelMap_[tag][model.trgTag].append(model);
                } else {
                    modelMap_[tag][model.trgTag] = {model};
                }
            } else {
                modelMap_[tag] = QMap<QString, QList<Model>>{{model.trgTag, {model}}};
            }
        }
    }
}

inline QPair<bool, Model> NativeMsgIface::findModelHelper(QString srctag, QString trgtag) {
    bool found = false;
    Model foundModel;
    if (modelMap_.find(srctag) != modelMap_.end()) {
        if (modelMap_[srctag].find(trgtag) != modelMap_[srctag].end()) {
            found = true;
            if (modelMap_[srctag][trgtag].size() > 1) { // Try to load the tiny model if we have multiple
                for (auto&& model : modelMap_[srctag][trgtag]) {
                    if (model.type == QString("tiny")) {
                        foundModel = model;
                    }
                }
            } else {
                foundModel = modelMap_[srctag][trgtag].first();
            }
        }
    }
    if (found && foundModel.path.isEmpty()) {
        // @TODO model downloading code and
        std::cerr << "Model downloading not implemented yet" << std::endl;
    }
    return {found, foundModel};
}

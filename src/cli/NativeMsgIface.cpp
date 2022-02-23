#include "NativeMsgIface.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QSet>

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
      , eventLoop_(this)
      , die_(false)
    {
    modelMapInit(models_.getInstalledModels());

    // Disable synchronisation with C style streams. That should make IO faster
    std::ios_base::sync_with_stdio(false);

    // Connections
    connect(&models_, &ModelManager::fetchedRemoteModels, this, [&](){eventLoop_.exit();});
    connect(&network_, &Network::downloadComplete, this, [&](QFile *file, QString filename) {
        // We use cout here, as QTextStream out gives a warning about being lamda captured.
        std::cerr << "Model downloaded successfully!" << std::endl;
        models_.writeModel(file, filename);
        eventLoop_.exit();
        // @TODO inform the local model map that this model is now downloaded, as this doesn't happen automatically
    });

    //inputWorker_ = std::thread([&](){
        // Connections
        //connect(&network_, &Network::downloadComplete, this, [&](QFile *file, QString filename) {
        //    models_.writeModel(file, filename);
        //    downloaded_ = true;
        //});
        // Create service

    //});

}

int NativeMsgIface::run() {
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
        // First part of the message: Find how long the input is
        char len[4];
        std::cin.read(len, 4);
        unsigned int ilen = *reinterpret_cast<unsigned int *>(len);
        if (ilen < kMaxInputLength && ilen>1) {
            //  Read in the message into Json
            std::unique_ptr<char[]> input(new char[ilen]);
            std::cin.read(input.get(), ilen);
            // Get JsonInput. It could be one of 4 RequestTypes: TranslationRequest, DownloadRequest, ListRequest and ParseRequest
            // All of them are handled by overloaded function handleRequest and std::visit does the dispatch by type.
            auto myJsonInputVariant = parseJsonInput(input.get(), ilen);
            std::visit([&](auto&& req){handleRequest(req, service.get());}, myJsonInputVariant);
        } else {
          // @TODO Consume any invalid input here
          std::cerr << "Unknown input, aboring for now. Will handle gracefully later" << std::endl;
          std::abort();
        }
    } while (!die_);
    return 0;
}

//inline void NativeMsgIface::translateAndReturn(QJso)

inline void NativeMsgIface::lockAndWriteJsonHelper(QByteArray&& arr) {
    std::lock_guard<std::mutex> lock(coutmutex_);
    size_t outputSize = arr.size();
    std::cout.write(reinterpret_cast<char*>(&outputSize), 4);
    std::cout.write(arr.data(), outputSize);
    std::cout.flush();
}

inline void NativeMsgIface::handleRequest(TranslationRequest myJsonInput, marian::bergamot::AsyncService * service) {
    int myID = myJsonInput.id;

    // Initialise models based on the request.
    bool success = tryLoadModel(myJsonInput.src, myJsonInput.trg);
    if (!success) {
        lockAndWriteJsonHelper(errJson(myID, QString("Failed to load the necessary translation models.")));
    }

    // Initialise translator settings options
    marian::bergamot::ResponseOptions options;
    options.HTML = myJsonInput.html;
    std::function<void(marian::bergamot::Response&&)> callbackLambda = [&, myID](marian::bergamot::Response&& val) {
        lockAndWriteJsonHelper(toJsonBytes(std::move(val), myID));};

    // Attempt translation. Beware of runtime errors
    try {
        if (pivotModel_.first.first == QString("none")) {
            service->translate(model_.second, std::move(myJsonInput.text.toStdString()), callbackLambda, options);
        } else {
            service->pivot(model_.second, pivotModel_.second, std::move(myJsonInput.text.toStdString()), callbackLambda, options);
        }
    } catch (const std::runtime_error &e) {
       lockAndWriteJsonHelper(errJson(myID, QString::fromStdString(e.what())));
    }
}

inline void NativeMsgIface::handleRequest(ListRequest myJsonInput, marian::bergamot::AsyncService *)  {
    QJsonObject jsonObj;
    jsonObj.insert(QString("id"), myJsonInput.id);

    QJsonArray modelsJson;
    for (auto&& model : models_.getInstalledModels()) {
        modelsJson.append(model.toJson());
    }
    // Fetch remote models if necessary;
    if (myJsonInput.includeRemote && models_.getNewModels().isEmpty()) {
        models_.fetchRemoteModels();
        eventLoop_.exec();
        modelMapInit(models_.getNewModels());
        for (auto&& model : models_.getNewModels()) {
            modelsJson.append(model.toJson());
        }
    }
    jsonObj.insert(QString("data"), modelsJson);
    lockAndWriteJsonHelper(QJsonDocument(jsonObj).toJson());
}

inline void NativeMsgIface::handleRequest(DownloadRequest myJsonInput, marian::bergamot::AsyncService *)  {
    // Assume remote models are fetched already
    QString modelID = myJsonInput.modelID;
    // identify the model we want to download. This is a remote model and we have this complicated lambda function
    // because i wanted it to be a const & as opposed to a copy of a model
    const Model& model = [&]() {
        // Keep track of available models in case we have an error
        QString errorstr("Unable to find \'" + modelID + "\' in the list of available models. Available models:\n");
        for (const Model& model : models_.getRemoteModels()) {
            errorstr = errorstr + model.src + "-" + model.trg + " type: " + model.type + "; to fetch, make a request with " + model.shortName + '\n'; //@TODO properly jsonify
            if (model.shortName == modelID) {
                return model;
            }
        }
        lockAndWriteJsonHelper(errJson(myJsonInput.id, errorstr));
        return models_.getRemoteModels().at(0); // This line will never be executed, since we actually exit in the case of an error
    }();
    // Set up progress bar for downloading:
    std::once_flag flag1, flag2, flag3, flag4, flag5; // Incredibly complicated way to print print just 5 times during download
    connect(&network_, &Network::progressBar, this, [&](qint64 ist, qint64 max) {
        double percentage = (double)ist/(double)max;
        if (percentage > 0 && percentage < 0.2) {
            std::call_once(flag1, [=](){std::cerr << percentage << std::endl;});
        }
        if (percentage > 0.2 && percentage < 0.4) {
            std::call_once(flag2, [=](){std::cerr << percentage << std::endl;});
        }
        if (percentage > 0.4 && percentage < 0.6) {
            std::call_once(flag3, [=](){std::cerr << percentage << std::endl;});
        }
        if (percentage > 0.6 && percentage < 0.8) {
            std::call_once(flag4, [=](){std::cerr << percentage << std::endl;});
        }
        if (percentage > 0.8 && percentage < 1) {
            std::call_once(flag5, [=](){std::cerr << percentage << std::endl;});
        }
    });
    // Download the new model. Use eventloop again to prevent premature exit before download is finished
    QNetworkReply *reply = network_.downloadFile(model.url, QCryptographicHash::Sha256, model.checksum);
    if (reply == nullptr) {
        lockAndWriteJsonHelper(errJson(myJsonInput.id, QString("Could not connect to the internet and download: ") + model.url));
    }
    eventLoop_.exec();
    lockAndWriteJsonHelper(errJson(myJsonInput.id, QString("Not yet fully implemented. Success?")));

}

inline void NativeMsgIface::handleRequest(ParseError myJsonInput, marian::bergamot::AsyncService *)  {
    lockAndWriteJsonHelper(errJson(myJsonInput.id, myJsonInput.error));
}

request_variant NativeMsgIface::parseJsonInput(char * bytes, size_t length) {
    QByteArray inputBytes(bytes, length);
    QJsonDocument inputJson = QJsonDocument::fromJson(inputBytes);
    QJsonObject jsonObj = inputJson.object();

    // Define what are mandatory and what are optional request keys
    static const QStringList mandatoryKeys({"command", "id", "data"}); // Expected in every message
    static const QSet<QString> commandTypes({"ListModels", "DownloadModel", "Translate"});
    // Json doesn't have schema validation, so validate here, in place:
    QString command;
    int id;
    QJsonObject data;
    {
        QJsonValueRef idVariant = jsonObj["id"];
        if (idVariant.isNull()) {
            return ParseError{-1, "ID field in message cannot be null!"};
        } else {
            id = idVariant.toInt();
        }

        QJsonValueRef commandVariant = jsonObj["command"];
        if (commandVariant.isNull()) {
            return ParseError{id, "command field in message cannot be null!"};
        } else {
            command = commandVariant.toString();
            if (commandTypes.find(command) == commandTypes.end()) {
                return ParseError{id, QString("Unrecognised message command: ") + command + QString(" AvailableCommands: ") +
                commandTypes.values().at(0) + QString(" ") + commandTypes.values().at(1) + QString(" ") + commandTypes.values().at(2)};
            }
        }

        QJsonValueRef dataVariant = jsonObj["data"];
        if (dataVariant.isNull()) {
            return ParseError{id, "data field in message cannot be null!"};
        } else {
            data = dataVariant.toObject();
        }

    }

    if (command == "Translate") {
        // Keys expected in a translation request
        static const QStringList mandatoryKeysTranslate({"text", "html", "src", "trg"});
        static const QStringList optionalKeysTranslate({"chosemodel", "quality", "alignments"});
        TranslationRequest ret;
        ret.set("id", id);
        for (auto&& key : mandatoryKeysTranslate) {
            QJsonValueRef val = data[key];
            if (val.isNull()) {
                return ParseError{id, QString("data field key ") + key + QString(" cannot be null!")};
            } else {
                ret.set(key, val);
            }
        }
        for (auto&& key : optionalKeysTranslate) {
            QJsonValueRef val = data[key];
            if (!val.isNull()) {
                ret.set(key, val);
            }
        }
        return ret;
    } else if (command == "ListModels") {
        // Keys expected in a list requested
        static const QStringList optionalKeysList({"includeRemote"});
        ListRequest ret;
        ret.id = id;
        for (auto&& key : optionalKeysList) {
            QJsonValueRef val = data[key];
            if (!val.isNull()) {
                ret.includeRemote = val.toBool();
            }
        }
        return ret;
    } else if (command == "DownloadModel") {
        // Keys expected in a download request:
        static const QStringList mandatoryKeysDownload({"modelID"});
        DownloadRequest ret;
        ret.id = id;
        for (auto&& key : mandatoryKeysDownload) {
            QJsonValueRef val = data[key];
            if (val.isNull()) {
                return ParseError{id, QString("data field key ") + key + QString(" cannot be null!")};
            } else {
                ret.modelID = val.toString();
            }
        }
        return ret;
    } else {
        return ParseError{id, QString("Developer error. We shouldn't ever be here! Command: ") + command};
    }

    return ParseError{id, QString("Developer error. We shouldn't ever be here! This makes the compiler happy though.")};

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

    if (!candidate.first) {
        // We didn't find a model. Try pivoting now.
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

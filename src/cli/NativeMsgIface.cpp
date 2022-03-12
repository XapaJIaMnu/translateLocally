#include "NativeMsgIface.h"
#include <cassert>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSet>
#include <QThread>
#include <QAbstractEventDispatcher>
#include <QDebug>
#include <optional>
#include <QNetworkReply>

// bergamot-translator
#include "3rd_party/bergamot-translator/src/translator/service.h"
#include "3rd_party/bergamot-translator/src/translator/parser.h"
#include "3rd_party/bergamot-translator/src/translator/response.h"

namespace  {

// Helper type for using std::visit() with multiple visitor lambdas. Copied
// from the C++ reference.
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };

// Explicit deduction guide (not needed as of C++20)
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

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
      , operations_(0)
      , die_(false)
    {
    qRegisterMetaType<std::shared_ptr<char[]>>("std::shared_ptr<char[]>");
    
    // Disable synchronisation with C style streams. That should make IO faster
    std::ios_base::sync_with_stdio(false);

    // Init the marian translation service:
    marian::bergamot::AsyncService::Config serviceConfig;
    serviceConfig.numWorkers = settings_.marianSettings().cpu_threads;
    serviceConfig.cacheSize = settings_.marianSettings().translation_cache ? kTranslationCacheSize : 0;
    service_ = std::make_shared<marian::bergamot::AsyncService>(serviceConfig);

    // Connections
    connect(&network_, &Network::error, this, [&](QString err, QVariant id) {
        int messageID = -1;
        if (!id.isNull()) {
            if (id.canConvert<int>()) {
                messageID = id.toInt();
            } else if (id.canConvert<QMap<QString, QVariant>>()) {
                messageID = id.toMap()["id"].toInt();
            }
        }
        lockAndWriteJsonHelper(errJson(messageID, err));
    });
    // Another reason to love QT. From the documentation:
    // The signature of a signal must match the signature of the receiving slot. (In fact a slot may have a shorter signature than the signal it receives because it can ignore extra arguments.)
    connect(&models_, &ModelManager::fetchedRemoteModels, this, [&](QVariant myID=QVariant()){
        if (!myID.isNull()) {
            QJsonArray modelsJson;
            for (auto&& model : models_.getInstalledModels()) {
                modelsJson.append(model.toJson());
            }
            for (auto&& model : models_.getNewModels()) {
                modelsJson.append(model.toJson());
            }
            QJsonObject jsonObj {
                {"success", true},
                {"id", myID.toInt()},
                {"data", modelsJson}
            };
            lockAndWriteJsonHelper(QJsonDocument(jsonObj).toJson());
            
            // We are here because this is a helper call for download model, so do nothing. Download model will know what to do.
        }});
    operations_--;
    connect(&network_, &Network::downloadComplete, this, [&](QFile *file, QString filename, QVariant id) {
        // We use cout here, as QTextStream out gives a warning about being lamda captured.
        models_.writeModel(file, filename);
        int messageID = id.toMap()["id"].toInt();
        QString modelID = id.toMap()["modelID"].toString();
        QJsonObject jsonObj {
            {"success", true},
            {"id", messageID},
            {"data", QJsonObject{{"modelID", modelID}}}
        };
        lockAndWriteJsonHelper(QJsonDocument(jsonObj).toJson());
        operations_--;
        operations_--;
        //@TODO update model map so newly downloaded model is marked as local now.
    });
    connect(this, &NativeMsgIface::emitJson, this, &NativeMsgIface::processJson);
}

void NativeMsgIface::run() {
    iothread_ = std::thread([this](){
        do {
            if ((std::cin.peek() == std::char_traits<char>::eof())) {
                // Send a final package telling other stuff to die
                die_ = true;
            }
            // First part of the message: Find how long the input is
            char len[4];
            std::cin.read(len, 4);
            int ilen = *reinterpret_cast<unsigned int *>(len);
            if (ilen < kMaxInputLength && ilen>1) {
                //  Read in the message into Json
                std::shared_ptr<char[]> input(new char[ilen]);
                std::cin.read(input.get(), ilen);
                // Get JsonInput. It could be one of 4 RequestTypes: TranslationRequest, DownloadRequest, ListRequest and ParseRequest
                // All of them are handled by overloaded function handleRequest and std::visit does the dispatch by type.
                operations_++; // Also keep track of the number of operations that are happening.
                emit emitJson(input, ilen);
            } else if (ilen == -1) { // We signal the worker to die if the length of the next message is -1.
                die_ = true;
                break;
            } else {
              // @TODO Consume any invalid input here
              std::cerr << "Unknown input, aboring for now. Will handle gracefully later" << std::endl;
              std::abort();
            }
        } while (!die_);
        std::unique_lock<std::mutex> lck(pendingOpsMutex_);
        pendingOpsCV_.wait(lck, [this](){
            while (operations_!=0) { //@TODO notify.
                std::cerr << "Ops remaining: " << operations_ <<  std::endl;
                QThread::sleep(1);
            }
            return operations_ == 0;});
        emit finished();
    });
}

inline void NativeMsgIface::lockAndWriteJsonHelper(QByteArray&& arr) {
    std::lock_guard<std::mutex> lock(coutmutex_);
    size_t outputSize = arr.size();
    std::cout.write(reinterpret_cast<char*>(&outputSize), 4);
    std::cout.write(arr.data(), outputSize);
    std::cout.flush();
}

inline void NativeMsgIface::handleRequest(TranslationRequest myJsonInput) {
    int myID = myJsonInput.id;

    // Initialise models based on the request.
    bool success = tryLoadModel(myJsonInput.src, myJsonInput.trg);
    if (!success) {
        lockAndWriteJsonHelper(errJson(myID, QString("Failed to load the necessary translation models.")));
        operations_--;
        return;
    }

    // Initialise translator settings options
    marian::bergamot::ResponseOptions options;
    options.HTML = myJsonInput.html;
    std::function<void(marian::bergamot::Response&&)> callbackLambda = [&, myID](marian::bergamot::Response&& val) {
        lockAndWriteJsonHelper(toJsonBytes(std::move(val), myID));
        operations_--;};

    // Attempt translation. Beware of runtime errors
    try {
        std::visit(overloaded {
            [&](DirectModelInstance &model) {
                service_->translate(model.model, std::move(myJsonInput.text.toStdString()), callbackLambda, options);
            },
            [&](PivotModelInstance &model) {
                service_->pivot(model.model, model.pivot, std::move(myJsonInput.text.toStdString()), callbackLambda, options);
            }
        }, *model_);
    } catch (const std::runtime_error &e) {
       lockAndWriteJsonHelper(errJson(myID, QString::fromStdString(e.what())));
       operations_--;
    }
}

inline void NativeMsgIface::handleRequest(ListRequest myJsonInput)  {
    // Fetch remote models if necessary. In this case we report the models via signal
    if (myJsonInput.includeRemote && models_.getNewModels().isEmpty()) {
        models_.fetchRemoteModels(myJsonInput.id);
    } else {
        QJsonArray modelsJson;
        for (auto&& model : models_.getInstalledModels()) {
            modelsJson.append(model.toJson());
        }
        for (auto&& model : models_.getNewModels()) {
            modelsJson.append(model.toJson());
        }
        QJsonObject jsonObj {
            {"success", true},
            {"id", myJsonInput.id},
            {"data", modelsJson}
        };
        lockAndWriteJsonHelper(QJsonDocument(jsonObj).toJson());
        operations_--;
    }

}

inline void NativeMsgIface::handleRequest(DownloadRequest myJsonInput)  {
    // Connection to store our callback in. Qt6 has single shot connections, but 5 doesn't.
    QMetaObject::Connection * const connection = new QMetaObject::Connection;

    auto downloadModelLambda = std::function([=](){
        // Remove the connection right after we fetch model, if the connection is valid
        if (*connection)
            QObject::disconnect(*connection);

        // Clean up our callback connection
        // TODO: abstract this all away in a connectSingleShot() helper?
        delete connection;
        
        auto model = models_.getModel(myJsonInput.modelID);
        if (!model) {
            operations_--;
            lockAndWriteJsonHelper(errJson(myJsonInput.id, QString("Model not found")));
            return;
        }

        // Model already downloaded?
        // TODO: same logic as in downloadComplete signal handler
        if (model->isLocal()) {
            QJsonObject jsonObj {
                {"success", true},
                {"id", myJsonInput.id},
                {"data", QJsonObject{{"modelID", model->id()}}}
            };
            operations_--;
            lockAndWriteJsonHelper(QJsonDocument(jsonObj).toJson());
            return;
        }

        // Download new model
        QNetworkReply *reply = network_.downloadFile(model->url, QCryptographicHash::Sha256, model->checksum, QVariant::fromValue(QMap<QString, QVariant>({
            {"id", myJsonInput.id},
            {"modelID", model->id()}
        })));

        // downloadFile can return nullptr if it can't open the temp file. In
        // that case it will also emit an Network::error(QString) signal which
        // we already handle above.
        if (!reply)
            return;

        // Pass any download progress updates along to the client. Previous implementation
        // has some logic to limit the amount of progress updates to 6 but I don't
        // think throttling the number of messages is necessary.
        connect(reply, &QNetworkReply::downloadProgress, this, [=](qint64 ist, qint64 max) {
             QJsonObject jsonObj {
                {"update", true},
                {"id", myJsonInput.id},
                {"data", QJsonObject{
                    {"read", ist},
                    {"size", max},
                    {"url", model->url},
                    {"modelID", model->id()}
                }}
            };
            lockAndWriteJsonHelper(QJsonDocument(jsonObj).toJson());
        });
    });

    // We can't download a model, if the fetch operation is not completed yet. as we don't have a list of remote models
    // loaded at start. This shouldn't be an issue normally as we would only fetch a new model once we get a list of models,
    // but we also need to wait for the fetch model list network operation to finish. We should also be prepared for a clinet having
    // a cached list of models. Unfortunately, as all the signals and slots are executed on the same thread (unless we further complicate the code)
    // We can't have a lock waiting on slot from the same thread because no work will ever be done unless QThread::currentThread()->eventDispatcher()->processEvents(QEventLoop::AllEvents)
    // is called and that is extremely hacky. Instead, chain a signal call based on the signal emitted from fetchRemoteModels();. If fetchRemoteModels() is being executted currently, it
    // will just exit. That should ensure that a download wouldn't happen twice.
    // Lambda captures by copy as things could potentially go out of scope and be de-allocated.
    if (models_.getNewModels().isEmpty()) {
        *connection = connect(&models_, &ModelManager::fetchedRemoteModels, this, downloadModelLambda);
        models_.fetchRemoteModels(); // Fetch Remote models will trigger download as well.
    } else {
        // In this case we know that models are fetched so we can proceed to download directly
        downloadModelLambda();
    }
}

inline void NativeMsgIface::handleRequest(ParseError myJsonInput)  {
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
    QJsonObject jsonObj{
        {"id", myID},
        {"success", false},
        {"error", err}
    };
    QByteArray bytes = QJsonDocument(jsonObj).toJson();
    return bytes;
}

inline QByteArray NativeMsgIface::toJsonBytes(marian::bergamot::Response&& response, int myID) {
    QJsonObject jsonObj{
        {"id", myID},
        {"success", true},
        {"data", QJsonObject{
            {"target", QJsonObject{
                {"text", QString::fromStdString(response.target.text)}    
            }}
        }}
    };
    QByteArray bytes = QJsonDocument(jsonObj).toJson();
    return bytes;
}

bool NativeMsgIface::tryLoadModel(QString srctag, QString trgtag) {
    // First, check if we have everything required already loaded:
    if (model_ && std::visit([&](auto &&instance) { return instance.src == srctag && instance.trg == trgtag; }, *model_))
        return true;

    std::optional<Model> directModel = models_.getModelForLanguagePair(srctag, trgtag);
    if (directModel && directModel->isLocal()) {
        model_ = DirectModelInstance{
            srctag,
            directModel->trgTag,
            std::make_shared<marian::bergamot::TranslationModel>(makeOptions(directModel->path.toStdString(), settings_.marianSettings()), settings_.marianSettings().cpu_threads)
            // TODO: Maybe cache these shared ptrs? With a weakptr? They might still be around in the
            // translation queue even when we switched. No need to load them again.
        };
        return true;
    }

    std::optional<ModelPair> pivotModel = models_.getModelPairForLanguagePair(srctag, trgtag);
    if (pivotModel && pivotModel->model.isLocal() && pivotModel->pivot.isLocal()) {
        model_ = PivotModelInstance{
            srctag,
            pivotModel->pivot.trgTag,
            std::make_shared<marian::bergamot::TranslationModel>(
                makeOptions(pivotModel->model.path.toStdString(), settings_.marianSettings()),
                settings_.marianSettings().cpu_threads),
            std::make_shared<marian::bergamot::TranslationModel>(
                makeOptions(pivotModel->pivot.path.toStdString(), settings_.marianSettings()),
                settings_.marianSettings().cpu_threads)
        };
        return true;
    }

    // TODO download directModel if it is non-local? And last resort, download
    // the pivotModel models if they're not local?

    return false;
}

void NativeMsgIface::processJson(std::shared_ptr<char[]> input, int ilen) {
    auto myJsonInputVariant = parseJsonInput(input.get(), ilen);
    std::visit([&](auto&& req){handleRequest(req);}, myJsonInputVariant);
}

NativeMsgIface::~NativeMsgIface() {
    if (iothread_.joinable()) {
        iothread_.join();
    }
}

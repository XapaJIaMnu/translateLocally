#include "NativeMsgIface.h"
#include <cassert>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSet>
#include <QThread>
#include <QAbstractEventDispatcher>
#include <memory>
#include <mutex>
#include <optional>
#include <QNetworkReply>

// bergamot-translator
#include "3rd_party/bergamot-translator/src/translator/service.h"
#include "3rd_party/bergamot-translator/src/translator/parser.h"
#include "3rd_party/bergamot-translator/src/translator/response.h"
#include "inventory/ModelManager.h"
#include "translator/translation_model.h"

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

// Little helper function that sets up a SingleShot connection in both Qt 5 and 6
template <typename Sender, typename Emitter, typename Slot, typename... Args>
QMetaObject::Connection connectSingleShot(Sender *sender, void (Emitter::*signal)(Args ...args), const QObject *context, Slot slot) {
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    std::shared_ptr<QMetaObject::Connection> connection = std::make_shared<QMetaObject::Connection>();
    return *connection = QObject::connect(sender, signal, context, [=](Args ...args) {
        QObject::disconnect(*connection);
        slot(std::forward<Args>(args)...);
    });
#else
    return QObject::connect(sender, signal, context, slot, Qt::SingleShotConnection);
#endif
}

// Little helper to print QSet<QString> and QList<QString> without the need to
// convert them into a QStringList.
template <typename T>
QString join(QString glue, T const &list) {
    QString out;
    for (auto &&item : list) {
        if (!out.isEmpty())
            out += glue;
        out += item;
    }
    return out;
};

}

NativeMsgIface::NativeMsgIface(QObject * parent) :
      QObject(parent)
      , network_(this)
      , settings_(this)
      , models_(this, &settings_)
      , operations_(0)
    {
    qRegisterMetaType<std::shared_ptr<std::vector<char>>>("std::shared_ptr<std::vector<char>>");
    
    // Disable synchronisation with C style streams. That should make IO faster
    std::ios_base::sync_with_stdio(false);

    // Init the marian translation service:
    marian::bergamot::AsyncService::Config serviceConfig;
    serviceConfig.numWorkers = settings_.marianSettings().cpu_threads;
    serviceConfig.cacheSize = settings_.marianSettings().translation_cache ? kTranslationCacheSize : 0;
    service_ = std::make_shared<marian::bergamot::AsyncService>(serviceConfig);

    // Pick up on network errors: Right now these are only caused by DownloadRequest
    // because of how Network.h is implemented. But in the future it might be that
    // fetchRemoteModels() might also hook into this, and those can yield multiple
    // errors for one request (e.g. multiple model repositories.)
    connect(&network_, &Network::error, this, [&](QString err, QVariant data) {
        if (data.canConvert<Request>())
            writeError(data.value<Request>(), std::move(err));
        else 
            qDebug() << "Network error without request data:" << err;
    });

    connect(&network_, &Network::downloadComplete, this, [this](QFile *file, QString filename, QVariant data) {
        ABORT_UNLESS(data.canConvert<DownloadRequest>(), "Model download completed without DownloadRequest data");
        auto model = models_.writeModel(file, filename);
        if (model) {
            DownloadRequest request = data.value<DownloadRequest>();
            writeResponse(request, model->toJson());
        }
    });

    // Model manager errors are not always 1-on-1 mappable to requests. For now
    // we'll just forward them to stderr.
    connect(&models_, &ModelManager::error, this, [this](QString err) {
        qDebug() << "Error from model manager:" << err;
    });

    connect(this, &NativeMsgIface::emitJson, this, &NativeMsgIface::processJson);
}

void NativeMsgIface::run() {
    iothread_ = std::thread([this](){
        for (;;) {
            // First part of the message: Find how long the input is. If that
            // read fails, we're probably at EOF.
            char len[4];
            if (!std::cin.read(len, 4))
                break;

            int ilen = *reinterpret_cast<unsigned int *>(len);
            if (ilen >= kMaxInputLength || ilen < 2) { // >= 2 because JSON is at least "{}"
                std::cerr << "Invalid message size. Shutting down." << std::endl;
                break;
            }

            //  Read in the message into Json
            std::shared_ptr<std::vector<char>> input(std::make_shared<std::vector<char>>(ilen));
            if (!std::cin.read(&input->front(), ilen)) {
                std::cerr << "Error while reading input message of length " << ilen << ". Shutting down." << std::endl;
                break;
            }

            // Keep track of the number of pending operations so we can wait for them to
            // all finish before we shut down the main thread.
            operations_++;

            emit emitJson(input);
        }

        // Here we lock the reading thread until all work is completed because
        // the NativeMsgIface destructor blocks on this thread finishing. Bit
        // convoluted at the moment.
        std::unique_lock<std::mutex> lck(pendingOpsMutex_);
        pendingOpsCV_.wait(lck, [this](){ return operations_ == 0; });
        emit finished();
    });
}

void NativeMsgIface::handleRequest(TranslationRequest request) {
    // Initialise models based on the request.
    if (!findModels(request))
        return writeError(request, "Could not find the necessary translation models.");

    if (!loadModels(request))
        return writeError(request, "Failed to load the necessary translation models.");

    // Initialise translator settings options
    marian::bergamot::ResponseOptions options;
    options.HTML = request.html;
    std::function<void(marian::bergamot::Response&&)> callback = [this,request](marian::bergamot::Response&& val) {
        QJsonObject data = {
            {"target", QJsonObject{
                {"text", QString::fromStdString(std::move(val.target.text))}
            }}
        };
        writeResponse(request, std::move(data));
    };

    // Attempt translation. Beware of runtime errors
    try {
        std::visit(overloaded {
            [&](DirectModelInstance &model) {
                service_->translate(model.model, std::move(request.text.toStdString()), callback, options);
            },
            [&](PivotModelInstance &model) {
                service_->pivot(model.model, model.pivot, std::move(request.text.toStdString()), callback, options);
            }
        }, *model_);
    } catch (const std::runtime_error &e) {
        writeError(request, QString::fromStdString(std::move(e.what())));
    }
}

void NativeMsgIface::handleRequest(ListRequest request)  {
    // Fetch remote models if necessary.
    if (request.includeRemote && models_.getRemoteModels().isEmpty()) {
        // Note: this might pick up the completion of an earlier fetchRemoteModels()
        // request but that's okay since fetchRemoteModels() returns early if a
        // fetch is still in progress. Also, fetchedRemoteModels() is called
        // regardless of whether errors occurred during the fetching.
        connectSingleShot(&models_, &ModelManager::fetchedRemoteModels, this, [this, request]([[maybe_unused]] QVariant ignored) {
            handleRequest(request);
        });
        return models_.fetchRemoteModels();
    }
    
    QJsonArray modelsJson;
    for (auto&& model : models_.getInstalledModels()) {
        modelsJson.append(model.toJson());
    }
    
    if (request.includeRemote) {
        for (auto&& model : models_.getNewModels()) {
            modelsJson.append(model.toJson());
        }
    }
    
    writeResponse(request, modelsJson);
}

void NativeMsgIface::handleRequest(DownloadRequest request)  {
    // Edge case: client issued a DownloadRequest before fetching the list of
    // remote models because it knows the model ID from a previous run. We still
    // need to dowload the remote model list to figure out which model it wants.
    // We thus fire off a fetchRemoteModels() request, and re-handle the
    // DownloadRequest from its "done!" signal.
    if (models_.getRemoteModels().isEmpty()) {
        // Note: this could pick up the signal emitted from a previous request
        // to fetch the model list. But that's okay, because fetchRemoteModels()
        // just returns if a request is still in progress.
        connectSingleShot(&models_, &ModelManager::fetchedRemoteModels, this, [this, request]([[maybe_unused]] QVariant ignored) {
            handleRequest(request);
        });
        models_.fetchRemoteModels();
        return;
    }

    auto model = models_.getModel(request.modelID);
    if (!model)
        return writeError(request, "Model not found");

    // Model already downloaded?
    if (model->isLocal()) {
        QJsonObject response{{"modelID", model->id()}};
        return writeResponse(request, response);
    }

    // Download new model
    QNetworkReply *reply = network_.downloadFile(model->url, QCryptographicHash::Sha256, model->checksum, QVariant::fromValue(request));

    // downloadFile can return nullptr if it can't open the temp file. In
    // that case it will also emit an Network::error(QString) signal which
    // we already handle above.
    if (!reply)
        return;

    // Pass any download progress updates along to the client.
    connect(reply, &QNetworkReply::downloadProgress, this, [=](qint64 ist, qint64 max) {
         QJsonObject update {
            {"read", ist},
            {"size", max},
            {"url", model->url},
            {"id", model->id()}
        };
        writeUpdate(request, update);
    });

    // Network::downloadComplete() or Network::error() will trigger the writeResponse or writeError for this request.
}

void NativeMsgIface::handleRequest(MalformedRequest request)  {
    writeError(request, std::move(request.error));
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
            return MalformedRequest{-1, "ID field in message cannot be null!"};
        } else {
            id = idVariant.toInt();
        }

        QJsonValueRef commandVariant = jsonObj["command"];
        if (commandVariant.isNull()) {
            return MalformedRequest{id, "command field in message cannot be null!"};
        } else {
            command = commandVariant.toString();
            if (commandTypes.find(command) == commandTypes.end()) {
                return MalformedRequest{id, QString("Unrecognised message command: %1 AvailableCommands: %2").arg(command).arg(join(" ", commandTypes))};
            }
        }

        QJsonValueRef dataVariant = jsonObj["data"];
        if (dataVariant.isNull()) {
            return MalformedRequest{id, "data field in message cannot be null!"};
        } else {
            data = dataVariant.toObject();
        }

    }

    if (command == "Translate") {
        // Keys expected in a translation request
        static const QStringList mandatoryKeysTranslate({"text"});
        static const QStringList optionalKeysTranslate({"html", "quality", "alignments", "src", "trg", "model", "pivot"});
        TranslationRequest ret;
        ret.set("id", id);
        for (auto&& key : mandatoryKeysTranslate) {
            QJsonValueRef val = data[key];
            if (val.isNull()) {
                return MalformedRequest{id, QString("data field key %1 cannot be null!").arg(key)};
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
        if ((!ret.src.isEmpty() && !ret.trg.isEmpty()) == (!ret.model.isEmpty())) {
            return MalformedRequest{id, QString("either the data fields src and trg, or the field model has to be specified")};
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
                return MalformedRequest{id, QString("data field key %1 cannot be null!").arg(key)};
            } else {
                ret.modelID = val.toString();
            }
        }
        return ret;
    } else {
        return MalformedRequest{id, QString("Developer error. We shouldn't ever be here! Command: %1").arg(command)};
    }

    return MalformedRequest{id, QString("Developer error. We shouldn't ever be here! This makes the compiler happy though.")};

}

void NativeMsgIface::lockAndWriteJsonHelper(QJsonDocument&& document) {
    QByteArray arr = document.toJson();
    std::lock_guard<std::mutex> lock(coutmutex_);
    size_t outputSize = arr.size();
    std::cout.write(reinterpret_cast<char*>(&outputSize), 4);
    std::cout.write(arr.data(), outputSize);
    std::cout.flush();
}

// Fills in the TranslationRequest.{model,pivot} parameters if src + trg are specified.
bool NativeMsgIface::findModels(TranslationRequest &request) const {
    if (!request.model.isEmpty())
        return true;

    if (std::optional<Model> directModel = models_.getModelForLanguagePair(request.src, request.trg)) {
        request.model = directModel->id();
        return true;
    }

    if (std::optional<ModelPair> pivotModel = models_.getModelPairForLanguagePair(request.src, request.trg)) {
        request.model = pivotModel->model.id();
        request.pivot = pivotModel->pivot.id();
        return true;
    }

    return false;
}

bool NativeMsgIface::loadModels(TranslationRequest const &request) {
    // First, check if we have everything required already loaded:
    if (model_ && std::visit(overloaded {
        [&](DirectModelInstance const &instance) { return instance.modelID == request.model && request.pivot.isEmpty(); },
        [&](PivotModelInstance const &instance) { return instance.modelID == request.model && instance.pivotID == request.pivot; }
    }, *model_))
        return true;

    if (!request.model.isEmpty() && !request.pivot.isEmpty()) {
        auto model = models_.getModel(request.model);
        auto pivot = models_.getModel(request.pivot);

        if (!model || !pivot || !model->isLocal() || !pivot->isLocal())
            return false;

        model_ = PivotModelInstance{model->id(), pivot->id(), makeModel(*model), makeModel(*pivot)};
        return true;
    } else if (!request.model.isEmpty()) {
        auto model = models_.getModel(request.model);
        if (!model || !model->isLocal())
            return false;
        
        model_ = DirectModelInstance{model->id(), makeModel(*model)};
        return true;
    }

    return false; // Should not happen, because we called findModels first, right?
}

std::shared_ptr<marian::bergamot::TranslationModel> NativeMsgIface::makeModel(Model const &model) {
    // TODO: Maybe cache these shared ptrs? With a weakptr? They might still be around in the
    // translation queue even when we switched. No need to load them again.
    return std::make_shared<marian::bergamot::TranslationModel>(
        makeOptions(model.path.toStdString(), settings_.marianSettings()),
        settings_.marianSettings().cpu_threads
    );
}

void NativeMsgIface::processJson(std::shared_ptr<std::vector<char>> input) {
    auto myJsonInputVariant = parseJsonInput(&input->front(), input->size());
    std::visit([&](auto&& req){handleRequest(req);}, myJsonInputVariant);
}

NativeMsgIface::~NativeMsgIface() {
    if (iothread_.joinable()) {
        iothread_.join();
    }
}

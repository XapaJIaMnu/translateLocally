#pragma once
#include <iostream>

#include <QPair>
#include <optional>
#include <type_traits>
#include <QEventLoop>
#include <QJsonDocument>
#include "inventory/ModelManager.h"
#include "settings/Settings.h"
#include "MarianInterface.h"
#include "Translation.h"
#include "Network.h"
#include <memory>
#include <variant>

// If we include the actual header, we break QT compilation.
namespace marian {
    namespace bergamot {
    class AsyncService;
    class TranslationModel;
    class Response;
    }
}


const int constexpr kMaxInputLength = 10*1024*1024; // 10 MB limit on the input length via native messaging

struct Request {
    int id;
};

Q_DECLARE_METATYPE(Request);

struct TranslationRequest : public Request {
    QString src;
    QString trg;
    QString model;
    QString pivot;
    QString text;
    QString command;
    bool html{false};
    bool quality{false};
    bool alignments{false};


    inline void set(QString key, QJsonValueRef& val) {
        if (key == "src") { // String keys
            src = val.toString();
        } else if (key == "trg") {
            trg = val.toString();
        } else if (key == "model") {
            model = val.toString();
        } else if (key == "pivot") {
            pivot = val.toString();
        } else if (key == "text") {
            text = val.toString();
        } else if (key == "command") {
            command = val.toString();
        } else if (key == "id") { // Int keys
            id = val.toInt();
        } else if (key == "html") { // Bool keys
            html = val.toBool();
        } else if (key == "quality") {
            quality = val.toBool();
        } else if (key == "alignments") {
            alignments = val.toBool();
        } else {
            std::cerr << "Unknown key type. " << key.toStdString() << " Something is very wrong!" << std::endl;
        }
    }

    // Id may be set differently
    inline void set(QString key, int val) {
        if (key == "id") {
            id = val;
        } else {
            std::cerr << "Unknown key type. " << key.toStdString() << " Something is very wrong!" << std::endl;
        }
    }
};

Q_DECLARE_METATYPE(TranslationRequest);

struct ListRequest  : Request {
    bool includeRemote;
};

Q_DECLARE_METATYPE(ListRequest);

struct DownloadRequest : Request {
    QString modelID;
};

Q_DECLARE_METATYPE(DownloadRequest);

struct MalformedRequest : Request {
    QString error;
};

using request_variant = std::variant<TranslationRequest, ListRequest, DownloadRequest, MalformedRequest>;

struct DirectModelInstance {
    QString modelID;
    std::shared_ptr<marian::bergamot::TranslationModel> model;
};

struct PivotModelInstance {
    QString modelID;
    QString pivotID;
    std::shared_ptr<marian::bergamot::TranslationModel> model;
    std::shared_ptr<marian::bergamot::TranslationModel> pivot;
};

using ModelInstance = std::variant<DirectModelInstance,PivotModelInstance>;

class NativeMsgIface : public QObject {
    Q_OBJECT
public:
    explicit NativeMsgIface(QObject * parent=nullptr);
    ~NativeMsgIface();
public slots:
    void run();
private slots:
    /**
     * @brief emitJson emits a json msg (unprocessed as a char array) and its length. Shared ptr because signals and slots don't support move semantics
     * @param input char array of json
     * @param ilen length
     */
    void processJson(std::shared_ptr<std::vector<char>> input);
private:
    // Threading
    std::thread iothread_;
    //QEventLoop eventLoop_;
    std::mutex coutmutex_;
    std::atomic<int> operations_; // Keeps track of all operations. So that we know when to quit
    // Sadly we don't have C++20 on ubuntu 18.04, otherwise could use std::atomic<T>::wait
    std::mutex pendingOpsMutex_;
    std::condition_variable pendingOpsCV_;

    // Marian shared ptr. We should be using a unique ptr but including the actual header breaks QT compilation. Sue me.
    std::shared_ptr<marian::bergamot::AsyncService> service_;

    // TranslateLocally bits
    Settings settings_;
    Network network_;
    ModelManager models_;
    QMap<QString, QMap<QString, QList<Model>>> modelMap_;

    std::optional<ModelInstance> model_;

    bool die_;

    // Methods
    request_variant parseJsonInput(char * bytes, size_t length);
    QByteArray converTranslationTo(marian::bergamot::Response&& response, int myID);
    
    /**
     * @brief This function tries its best to identify an appropriate model for
     * the target language/languages. The id of the found model (and possibly
     * pivot model) will be filled in in the `request` and the function will
     * return `true`.
     * @param TranslationRequest request
     * @return whether we succeeded or not.
     */
    bool findModels(TranslationRequest &request) const;

    /**
     * @brief Loads the models specified in the request. Assumes `request.model`
     * and possibly `request.pivot` are filled in.
     * @param TranslationRequest request with `model` (and optionally `pivot`)
     * filled in.
     * @return Returns false if any of the necessary models is either not found
     * or not downloaded.
     */
    bool loadModels(TranslationRequest const &request);

    /**
     * @brief instantiates a model that will work with the service.
     * @returns model instance.
     */
    std::shared_ptr<marian::bergamot::TranslationModel> makeModel(Model const &model);

    /**
     * @brief lockAndWriteJsonHelper This function locks input stream and then writes the size and a json message after. It would be called in many places so it makes
     *                               sense to put the common bits here to avoid code duplication
     * @param arr QbyteArray json array
     */
    void lockAndWriteJsonHelper(QJsonDocument&& json);

    template <typename T> // T can be QJsonValue, QJsonArray or QJsonObject
    void writeResponse(Request const &request, T &&data) {
        // Decrement pending operation count
        operations_--;
        
        QJsonObject response = {
            {"success", true},
            {"id", request.id},
            {"data", std::move(data)}
        };
        lockAndWriteJsonHelper(QJsonDocument(std::move(response)));
    }

    template <typename T>
    void writeUpdate(Request const &request, T &&data) {
        QJsonObject response = {
            {"update", true},
            {"id", request.id},
            {"data", std::move(data)}
        };
        lockAndWriteJsonHelper(QJsonDocument(std::move(response)));
    }

    void writeError(Request const &request, QString &&err) {
        // Only writeResponse or writeError will decrement the counter, and thus
        // only one should be called once per request. We can verify this by
        // looking at the message ids in request, but that's too much runtime
        // checking. I did do it in debug code.
        operations_--;
        
        QJsonObject response{
            {"success", false},
            {"error", err}
        };

        // We have request.id == -1 if the error is that the message id could
        // not be parsed.
        if (request.id >= 0)
            response["id"] = request.id;

        lockAndWriteJsonHelper(QJsonDocument(std::move(response)));
    }

    /**
     * @brief handleRequest handles a request type translationRequest and writes to stdout
     * @param myJsonInput translationRequest
     */
    void handleRequest(TranslationRequest myJsonInput);

    /**
     * @brief handleRequest handles a request type ListRequest and writes to stdout
     * @param myJsonInput ListRequest
     */
    void handleRequest(ListRequest myJsonInput);

    /**
     * @brief handleRequest handles a request type DownloadRequest and writes to stdout
     * @param myJsonInput DownloadRequest
     */
    void handleRequest(DownloadRequest myJsonInput);

    /**
     * @brief handleRequest handles a request type MalformedRequest and writes to stdout
     * @param myJsonInput MalformedRequest
     */
    void handleRequest(MalformedRequest myJsonInput);
signals:
    void finished();
    /**
     * @brief emitJson emits a json msg (unprocessed as a char array) and its length. Shared ptr because signals and slots don't support move semantics
     * @param input char array of json
     * @param ilen length
     */
    void emitJson(std::shared_ptr<std::vector<char>> input);
};

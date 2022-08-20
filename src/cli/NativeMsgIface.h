#pragma once
#include <iostream>

#include <QPair>
#include <QScopedPointer>
#include <mutex>
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

/**
 * Incoming requests all extend Request which contains the client supplied message
 * id. This id is used in any reply to this request. See parseJsonInput() for the
 * code parsing JSON strings into one of the request structs. See all requests
 * that extend this struct below for their JSON format.
 * 
 * Generic request format:
 * {
 *   "id": int
 *   "command": str
 *   "data": {
 *     ... command specific fields
 *   }
 * }
 * 
 * Generic success response:
 * {
 *   "id": int same value as in the request
 *   "success": true,
 *   "data": {
 *     ... command specific fields
 *   }
 * }
 * 
 * Generic error response:
 * {
 *   "id": int same value as in the request
 *   "success": false
 *   "error": str error message
 * }
 * 
 * Generic update format:
 * {
 *   "id": int same value as in the request
 *   "update": true,
 *   "data": {
 *     ... command specific fields
 *   }
 * }
 */
struct Request {
    int id;
};

Q_DECLARE_METATYPE(Request);

/**
 * Request:
 * {
 *   "id": int
 *   "command": "Translate",
 *   "data": {
 *     EIHER 
 *      "src": str BCP-47 language code,
 *      "trg": str BCP-47 language code,
 *     OR
 *      "model": str model id,
 *      "pivot": str model id
 *     REQUIRED
 *      "text": str text to translate
 *     OPTIONAL
 *      "html": bool the input is HTML
 *      "quality": bool return quality scores
 *      "alignments" return token alignments
 *   }
 * }
 * 
 * Success response:
 * {
 *   "id": int,
 *   "success": true,
 *   "data": {
 *     "target": {
 *       "text": str
 *     } 
 *   }
 * }
 */
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

/**
 * List of available models.
 * 
 * Request:
 * {
 *   "id": int,
 *   "command": "ListModels",
 *   "data": {
 *     OPTIONAL
 *      "includeRemote": bool whether to fetch and include models that are available but not already downloaded
 *   }
 * }
 * 
 * Successful response:
 * {
 *   "id": int,
 *   "success": true,
 *   "data": [
 *     {
 *       "id": str,
 *       "shortname": str,
 *       "modelName": str,
 *       "local": bool whether the model is already downloaded
 *       "src": str full name of source language
 *       "trg": str full name of target language
 *       "srcTags": {
 *         [str]: str Map of BCP-47 and full language name of supported source languages
 *       }
 *       "trgTag": str BCP-47 tag of target language
 *       "type": str often "base" or "tiny"
 *       "repository": str
 *     }
 *     ...
 *   ]
 * }
 */
struct ListRequest  : Request {
    bool includeRemote;
};

Q_DECLARE_METATYPE(ListRequest);

/**
 * Request to download a model to the user's machine so it can be used.
 *
 * Request:
 * {
 *   "id": int,
 *   "command": "DownloadModel",
 *   "data": {
 *     "modelID": str value of `id` field from one of the non-local models returned by the ListModels request.
 *   }
 * }
 * 
 * Successful response:
 * {
 *   "id": int,
 *   "success": true,
 *   "data": {
 *     ... (See ListModels request for model fields)
 *   }
 * }
 * 
 * Download progress update:
 * {
 *   "id": int
 *   "update": true,
 *   "data": {
 *     "id": str model id
 *     "url": str url of model being downloaded (listed url, not final redirected url)
 *     "read": int bytes downloaded so far
 *     "size": int estimated total bytes to download
 *   }
 * }
 */
struct DownloadRequest : Request {
    QString modelID;
};

Q_DECLARE_METATYPE(DownloadRequest);

/**
 * Internal structure to handle a request that is missing a required field.
 */
struct MalformedRequest : Request {
    QString error;
};

using request_variant = std::variant<TranslationRequest, ListRequest, DownloadRequest, MalformedRequest>;

/**
 * Internal structure to cache a loaded direct model (i.e. no pivoting)
 */
struct DirectModelInstance {
    QString modelID;
    std::shared_ptr<marian::bergamot::TranslationModel> model;
};

/**
 * Internal structure to cache a loaded indirect model (i.e. needs to pivot)
 */
struct PivotModelInstance {
    QString modelID;
    QString pivotID;
    std::shared_ptr<marian::bergamot::TranslationModel> model;
    std::shared_ptr<marian::bergamot::TranslationModel> pivot;
};

/**
 * A loaded model
 */
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
     * @brief hooked to emitJson, called for every message that the native client
     * receives, parses its json into a Request using `parseJsonInput`, and then
     * calls the corresponding `handleRequest` overload.
     * @param input char array of json
     */
    void processJson(QByteArray input);

private:
    // Threading
    std::thread iothread_;
    //QEventLoop eventLoop_;
    std::mutex coutmutex_;
    
    // Sadly we don't have C++20 on ubuntu 18.04, otherwise could use std::atomic<T>::wait
    std::atomic<int> operations_; // Keeps track of all operations. So that we know when to quit
    std::mutex pendingOpsMutex_;
    std::condition_variable pendingOpsCV_;

    // Marian shared ptr. We should be using a unique ptr but including the actual header breaks QT compilation. Sue me.
    QScopedPointer<marian::bergamot::AsyncService> service_;
    //std::shared_ptr<marian::bergamot::AsyncService> service_;

    // TranslateLocally bits
    Settings settings_;
    Network network_;
    ModelManager models_;
    QMap<QString, QMap<QString, QList<Model>>> modelMap_;

    std::optional<ModelInstance> model_;

    // Methods
    request_variant parseJsonInput(QByteArray bytes);
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
     * @brief lockAndWriteJsonHelper This function locks input stream and then writes the size and a
     *                               json message after. It would be called in many places so it
     *                               makes sense to put the common bits here to avoid code duplication.
     * @param json QJsonDocument that will be stringified and written to stdout in a blocking fashion.
     */
    void lockAndWriteJsonHelper(QJsonDocument&& json);

    template <typename T> // T can be QJsonValue, QJsonArray or QJsonObject
    void writeResponse(Request const &request, T &&data) {
        // Decrement pending operation count
        operations_--;
        pendingOpsCV_.notify_one();
        
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
        pendingOpsCV_.notify_one();
        
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
    /**
     * @brief Emitted when input is closed and all the messages have been processed.
     */
    void finished();

    /**
     * @brief Internal signal that is emitted from the stdin reading thread whenever a full request message is read.
     * @param input QByteArray of the json message
     */
    void emitJson(QByteArray input);
};

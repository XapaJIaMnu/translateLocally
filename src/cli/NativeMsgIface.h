#pragma once
#include <iostream>

#include <QPair>
#include <optional>
#include <type_traits>
#include <QEventLoop>
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

struct TranslationRequest {
    QString src;
    QString trg;
    QString model;
    QString pivot;
    QString text;
    QString command;
    int id;
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

struct ListRequest {
    int id;
    bool includeRemote;
};

struct DownloadRequest {
    int id;
    QString modelID;
};

struct ParseError {
    int id;
    QString error;
};

using request_variant = std::variant<TranslationRequest, ListRequest, DownloadRequest, ParseError>;

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
    void processJson(std::shared_ptr<char[]> input, int ilen);
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
    inline QByteArray errJson(int myID, QString err);
    inline QByteArray toJsonBytes(marian::bergamot::Response&& response, int myID);
    
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
    inline void lockAndWriteJsonHelper(QByteArray&& arr);

    /**
     * @brief handleRequest handles a request type translationRequest and writes to stdout
     * @param myJsonInput translationRequest
     */
    inline void handleRequest(TranslationRequest myJsonInput);

    /**
     * @brief handleRequest handles a request type ListRequest and writes to stdout
     * @param myJsonInput ListRequest
     */
    inline void handleRequest(ListRequest myJsonInput);

    /**
     * @brief handleRequest handles a request type DownloadRequest and writes to stdout
     * @param myJsonInput DownloadRequest
     */
    inline void handleRequest(DownloadRequest myJsonInput);

    /**
     * @brief handleRequest handles a request type ParseError and writes to stdout
     * @param myJsonInput ParseError
     */
    inline void handleRequest(ParseError myJsonInput);
signals:
    void finished();
    /**
     * @brief emitJson emits a json msg (unprocessed as a char array) and its length. Shared ptr because signals and slots don't support move semantics
     * @param input char array of json
     * @param ilen length
     */
    void emitJson(std::shared_ptr<char[]> input, int ilen);
};

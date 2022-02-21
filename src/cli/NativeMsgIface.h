#pragma once
#include <iostream>

#include <QPair>
#include <type_traits>
#include <QEventLoop>
#include "inventory/ModelManager.h"
#include "settings/Settings.h"
#include "MarianInterface.h"
#include "Translation.h"
#include "Network.h"
#include <memory>

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
    QString chosenmodel;
    QString text;
    QString command;
    int id;
    bool html;
    bool quality;
    bool alignments;


    inline void set(QString key, QJsonValueRef& val) {
        if (key == "src") { // String keys
            src = val.toString();
        } else if (key == "trg") {
            trg = val.toString();
        } else if (key == "chosenmodel") {
            chosenmodel = val.toString();
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

class NativeMsgIface : public QObject {
    Q_OBJECT
public:
    explicit NativeMsgIface(QObject * parent=nullptr);
    int run();
private:

    // Threading
    std::thread inputWorker_;
    std::mutex coutmutex_;
    QEventLoop eventLoop_;

    bool fetched_ = false;
    std::mutex fetchModelsMutex;
    std::condition_variable fetchModelsCV;

    bool downloaded_ = false;
    std::mutex downloadModelsMutex;
    std::condition_variable downloadModelsCV;

    // TranslateLocally bits
    Settings settings_;
    Network network_;
    ModelManager models_;
    QMap<QString, QMap<QString, QList<Model>>> modelMap_;

    // Those are modified through tryLoadModel
    QPair<QPair<QString, QString>, std::shared_ptr<marian::bergamot::TranslationModel>> model_;
    QPair<QPair<QString, QString>, std::shared_ptr<marian::bergamot::TranslationModel>> pivotModel_;

    bool die_;

    // Methods
    request_variant parseJsonInput(char * bytes, size_t length);
    inline QByteArray errJson(int myID, QString err);
    inline QByteArray toJsonBytes(marian::bergamot::Response&& response, int myID);
    /**
     * @brief tryLoadModel This function tries its best to load an appropriate model for the target language/languages, including dowloading from ze Internet
     * @param srctag Tag of the source language
     * @param trgtag Tag of the target language
     * @return whether we succeeded or not.
     */
    bool tryLoadModel(QString srctag, QString trgtag);

    /**
     * @brief modelMapInit Populates the model map with either local or remote models
     * @param myModelList
     */
    void modelMapInit(QList<Model> myModelList);

    /**
     * @brief findModelHelper
     * @param srctag Tag of the source language
     * @param trgtag Tag of the target language
     * @return A pair of bool and a model if the model was found
     */
    inline QPair<bool, Model> findModelHelper(QString srctag, QString trgtag);

    /**
     * @brief lockAndWriteJsonHelper This function locks input stream and then writes the size and a json message after. It would be called in many places so it makes
     *                               sense to put the common bits here to avoid code duplication
     * @param arr QbyteArray json array
     */
    inline void lockAndWriteJsonHelper(QByteArray&& arr);

    /**
     * @brief handleRequest handles a request type translationRequest and writes to stdout
     * @param myJsonInput translationRequest
     * @param service
     */
    inline void handleRequest(TranslationRequest myJsonInput, marian::bergamot::AsyncService * service);

    /**
     * @brief handleRequest handles a request type ListRequest and writes to stdout
     * @param myJsonInput ListRequest
     */
    inline void handleRequest(ListRequest myJsonInput, marian::bergamot::AsyncService *);

    /**
     * @brief handleRequest handles a request type DownloadRequest and writes to stdout
     * @param myJsonInput DownloadRequest
     */
    inline void handleRequest(DownloadRequest myJsonInput, marian::bergamot::AsyncService *);

    /**
     * @brief handleRequest handles a request type ParseError and writes to stdout
     * @param myJsonInput ParseError
     */
    inline void handleRequest(ParseError myJsonInput, marian::bergamot::AsyncService *);
};

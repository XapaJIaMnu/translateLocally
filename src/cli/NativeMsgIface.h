#pragma once
#include <iostream>

#include <QPair>
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
    int id;
    bool html;
    bool quality;
    bool alignments;
    bool die;
};

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
    TranslationRequest parseJsonInput(char * bytes, size_t length);
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
};

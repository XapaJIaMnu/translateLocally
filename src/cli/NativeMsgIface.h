#pragma once
#include <iostream>

#include <QPointer>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>

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

    // TranslateLocally bits
    Settings settings_;
    Network network_;
    ModelManager models_;

    // Shared ptrs because unique pointers can't be used incomplete types and including marian breaks qt compilation
    std::shared_ptr<marian::bergamot::TranslationModel> model_;

    bool die_;

    // Methods
    TranslationRequest parseJsonInput(char * bytes, size_t length);
    inline QByteArray toJsonBytes(marian::bergamot::Response&& response, int myID);
};

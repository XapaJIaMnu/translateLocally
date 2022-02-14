#pragma once
#include <iostream>

#include <QPointer>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>

#include "cli/SafeQueue.h"
#include "inventory/ModelManager.h"
#include "settings/Settings.h"
#include "MarianInterface.h"
#include "Translation.h"
#include "Network.h"

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

struct TranslationResponse {
    std::string text; //@TODO this might actually
    std::string status;
    std::string err;
    int id;
};

class NativeMsgIface : public QObject {
    Q_OBJECT
public:
    explicit NativeMsgIface(QObject * parent=nullptr);
    int run();
public slots:
    void die();
private slots:
    void outputTranslation(Translation output);
    void outputError(QString err);
private:
    // Event loop that would wait until translation completes @TODO remove.
    QEventLoop eventLoop_;

    // Threading
    std::thread inputWorker_;
    std::thread outputWorker_;
    std::thread marianWorker_;
    SafeQueue<TranslationRequest> translationQueue_; // The bool tells us whether to die or not

    std::mutex m;
    std::condition_variable cv;

    // TranslateLocally bits
    Settings settings_;
    Network network_;
    ModelManager models_;

    // Marian bits
    QPointer<MarianInterface> translator_;

    bool die_;

    // Methods
    TranslationRequest parseJsonInput(char * bytes, size_t length);
};

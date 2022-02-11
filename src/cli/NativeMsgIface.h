#pragma once
#include <QThread>
#include <QPointer>
#include <iostream>
#include <QEventLoop>
#include "inventory/ModelManager.h"
#include "settings/Settings.h"
#include "MarianInterface.h"
#include "Translation.h"
#include "Network.h"

const int constexpr kMaxInputLength = 10*1024*1024; // 10 MB limit on the input length via native messaging

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
    // Event loop that would wait until translation completes
    QEventLoop eventLoop_;

    // TranslateLocally bits
    Settings settings_;
    Network network_;
    ModelManager models_;

    // Marian bits
    QPointer<MarianInterface> translator_;

    bool die_;
};

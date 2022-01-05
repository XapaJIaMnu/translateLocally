#ifndef MARIANINTERFACE_H
#define MARIANINTERFACE_H
#include <QString>
#include <QList>
#include <QObject>
#include <QMutex>
#include <QSemaphore>
#include "types.h"
#include "Translation.h"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <memory>

// If we include the actual header, we break QT compilation.
namespace marian {
    namespace bergamot {
    class Service;
    }
}

struct ModelDescription;

class MarianInterface : public QObject {
    Q_OBJECT
private:

    std::unique_ptr<std::string> pendingInput_;
    std::unique_ptr<ModelDescription> pendingModel_;
    bool pendingShutdown_;

    std::mutex mutex_;
    std::condition_variable cv_;

    std::thread worker_;
    QString model_;
public:
    MarianInterface(QObject * parent);
    ~MarianInterface();
    QString const &model() const;
    void setModel(QString path_to_model_dir, const translateLocally::marianSettings& settings);
    void translate(QString in);
signals:
    void translationReady(Translation translation);
    void pendingChanged(bool isBusy);
    void error(QString message);
};

#endif // MARIANINTERFACE_H

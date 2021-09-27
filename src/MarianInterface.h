#ifndef MARIANINTERFACE_H
#define MARIANINTERFACE_H
#include <QString>
#include <QObject>
#include <QMutex>
#include <QSemaphore>
#include "types.h"
#include <thread>
#include <memory>

// If we include the actual header, we break QT compilation.
namespace marian {
    namespace bergamot {
    class Service;
    class Response;
    }
}

struct ModelDescription;

class Translation {
private:
    std::shared_ptr<marian::bergamot::Response> response_;
    int speed_;
public:
    Translation(marian::bergamot::Response &&response, int speed);

    inline std::size_t wordsPerSecond() const {
        return speed_;
    }

    QString translation() const;
};

Q_DECLARE_METATYPE(Translation)

class MarianInterface : public QObject {
    Q_OBJECT
private:

    std::unique_ptr<std::string> pendingInput_;
    std::unique_ptr<ModelDescription> pendingModel_;
    QSemaphore commandIssued_;
    QMutex lock_;

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

#ifndef MARIANINTERFACE_H
#define MARIANINTERFACE_H
#include <QString>
#include <QObject>
#include <QAtomicInteger>
#include <memory>
#include "types.h"
#include <thread>

// If we include the actual header, we break QT compilation.
namespace marian {
    namespace bergamot {
    class Service;
    }
}

class MarianInterface : public QObject {
    Q_OBJECT
private:
    std::unique_ptr<marian::bergamot::Service> service_;
    QAtomicInteger<std::size_t> serial_;
    QAtomicInteger<std::size_t> finished_;
public:
    MarianInterface(QString path_to_model_dir, translateLocally::marianSettings& settings, QObject * parent);
    void translate(QString in);
    ~MarianInterface();
    const QString mymodel;
    bool pending() const;
signals:
    void translationReady(QString translation);
    void pendingChanged(bool isBusy);
};

#endif // MARIANINTERFACE_H

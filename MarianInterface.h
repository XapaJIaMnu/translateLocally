#ifndef MARIANINTERFACE_H
#define MARIANINTERFACE_H
#include <QString>
#include <QObject>
#include <memory>

// If we include the actual header, we break QT compilation.
namespace marian {
    namespace bergamot {
    class Service;
    }
}

class MarianInterface : public QObject {
    Q_OBJECT
public:
    MarianInterface(QString path_to_model_dir, QObject * parent);
    void translate(QString in);
    ~MarianInterface();
private:
    std::unique_ptr<marian::bergamot::Service> service_;
signals:
    void translationReady(QString);
};

#endif // MARIANINTERFACE_H

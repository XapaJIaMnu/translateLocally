#ifndef MARIANINTERFACE_H
#define MARIANINTERFACE_H
#include <QString>
#include <QList>
#include <QObject>
#include "types.h"
#include "Translation.h"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <memory>

struct ModelDescription;
struct TranslationInput;

constexpr const size_t kTranslationCacheSize = 1 << 16;

class MarianInterface : public QObject {
    Q_OBJECT
private:
    std::unique_ptr<TranslationInput> pendingInput_;
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
    void translate(QString in, bool HTML=false);
signals:
    void translationReady(Translation translation);
    void pendingChanged(bool isBusy); // Disables issuing another translation while we are busy.
    void error(QString message);
};

#endif // MARIANINTERFACE_H

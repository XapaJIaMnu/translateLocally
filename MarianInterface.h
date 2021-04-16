#ifndef MARIANINTERFACE_H
#define MARIANINTERFACE_H
#include <QString>
#include "3rd_party/bergamot-translator/src/translator/service.h"


class MarianInterface {
public:
    MarianInterface(QString path_to_model_dir);
    QString translate(QString in);
    ~MarianInterface();
private:
    marian::bergamot::Service service_;
};

#endif // MARIANINTERFACE_H

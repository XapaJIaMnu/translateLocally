#ifndef COMMANDLINEIFACE_H
#define COMMANDLINEIFACE_H

#include <QObject>
#include <QTextStream>
#include <QCommandLineParser>
#include <QPointer>
#include <QEventLoop>
#include "ModelManager.h"
#include "Settings.h"
#include "MarianInterface.h"
#include "Translation.h"

class CommandLineIface : public QObject {
    Q_OBJECT
private:
    const QCommandLineParser& parser_;

    ModelManager models_;
    QTextStream qcin_;
    QTextStream qcout_;
    QTextStream qcerr_;

    // Settings and translator:
    Settings settings_;
    QPointer<MarianInterface> translator_;

    // Event loop that would wait until translation completes
    QEventLoop eventLoop_;

    // do_once file in and file out
    QScopedPointer<QFile> infile_;
    QScopedPointer<QFile> outfile_;
    QScopedPointer<QTextStream> instream_;
    QScopedPointer<QTextStream> outstream_;

    static const int constexpr prefetchLines = 320;

    // Functions
    void printLocalModels();
    void doTranslation();
    inline QString fetchData();

public:
    CommandLineIface(QCommandLineParser&, QObject * parent = nullptr);
    ~CommandLineIface();
    int run();

private slots:
    void outputError(QString error);
    void outputTranslation(Translation output);
};

#endif // COMMANDLINEIFACE_H

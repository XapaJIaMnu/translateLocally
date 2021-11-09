#include "CommandLineIface.h"
#include <QFile>


CommandLineIface::CommandLineIface(QCommandLineParser& parser, QObject * parent) : QObject(parent), eventLoop_(this), parser_(parser), models_(this),
                                                                                   settings_(this), translator_(new MarianInterface(this)),
                                                                                   qcin_(stdin), qcout_(stdout), qcerr_(stderr) {
    connect(translator_, &MarianInterface::error, this, &CommandLineIface::outputError);
    connect(translator_, &MarianInterface::translationReady, this, &CommandLineIface::outputTranslation);
}

int CommandLineIface::run() {
    if (parser_.isSet("l")) {
        printLocalModels();
        return 0;
    } else if (parser_.isSet("m")) {
        QString model_shortname = parser_.value("model");

        // Try to find our model in the list of models
        QString modelpath("");
        for (auto&& model : models_.getInstalledModels()) {
            if (model.shortName == model_shortname) {
                modelpath = model.path;
            }
        }
        if (modelpath == "") {
            qcerr_ << "We could not find a model identified as: " << model_shortname << " . Use translateLocally -l to list available models or use the GUI to download some from the internet.\n";
            return 1;
        }
        // Init the translation model
        translator_->setModel(modelpath, settings_.marianSettings());
        doTranslation();
        return 0;
    } else {
        qcerr_ << "We are in command line mode, but there's nothing for us to do. Some control flow mistake maybe?\n";
        return 2;
    }
}

void CommandLineIface::printLocalModels() {
    QList<Model> localmodels = models_.getInstalledModels();
    for (auto&& model : localmodels) {
        qcout_ << model.src << "-" << model.trg << " type: " << model.type << " version: " << model.localversion << "; To invoke do -m " << model.shortName << '\n';
    }
}

/* This function is pseudo blocking, via an event loop.*/
void CommandLineIface::doTranslation() {
    // Find whether input is stdin or a file
    QString input;
    if (parser_.isSet("s")) {
        QFile inputFile(parser_.value("s"));
        if (inputFile.open(QIODevice::ReadOnly)) {
           QTextStream in(&inputFile);
           input = in.readAll(); // @TODO this should be more complicated batching thingie, but alas. Should work fine with most small inputs.
                                 // Alternative is to read it line by line but then it would be horribly inefficient as we would lose all batching.
           inputFile.close();
        } else {
            outputError(QString("Couldn't open input file: " + input));
        }
    } else {
        input = qcin_.readAll(); // @TODO again this should do some batching and allow for full cmd usage but I *really* don't want to re-implement marian...
                                 // Maybe expose the raw marian interface with their batcher, or the service which has some sort of CLI usage.
    }
    translator_->translate(input);

    // Start event loop to block unti translation is ready:
    eventLoop_.exec();
}

void CommandLineIface::outputError(QString error) {
    qcerr_ << error << "\n";
    exit(22);
}

void CommandLineIface::outputTranslation(Translation output) {
    // Find whether output is stdin or a file
    if (parser_.isSet("t")) {
        QFile outputFile(parser_.value("t"));
        if (outputFile.open(QIODevice::WriteOnly)) {
            QTextStream out(&outputFile);
            out << output.translation();
        } else {
            outputError(QString("Couldnl't open output file: " + output));
        }
    } else {
        qcout_ << output.translation();
    }
    eventLoop_.exit();
}

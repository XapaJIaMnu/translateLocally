#include "CommandLineIface.h"
#include <QFile>

#include <array>

CommandLineIface::CommandLineIface(QObject * parent)
: QObject(parent)
, eventLoop_(this)
, models_(this)
, settings_(this)
, translator_(new MarianInterface(this))
, instream_(stdin)
, outstream_(stdout) {
    // Take care of slots and signals
    connect(translator_, &MarianInterface::error, this, &CommandLineIface::outputError);
    connect(translator_, &MarianInterface::translationReady, this, &CommandLineIface::outputTranslation);
}

int CommandLineIface::run(QCommandLineParser const &parser) {
    if (parser.isSet("l")) {
        printLocalModels();
        return 0;
    } else if (parser.isSet("m")) {
        // Open file as input stream if necessary
        if (parser.isSet("i")) {
            infile_.setFileName(parser.value("i"));
            if (infile_.open(QIODevice::ReadOnly)) {
                instream_.setDevice(&infile_);
            } else {
                qCritical() << "Couldn't open input file:" + parser.value("i");
                return 3;
            }
        }

        // Same, but output stream
        if (parser.isSet("o")) {
            outfile_.setFileName(parser.value("o"));
            if (outfile_.open(QIODevice::WriteOnly)) {
                outstream_.setDevice(&outfile_);
            } else {
                qCritical() << "Couldn't open output file:" + parser.value("o");
                return 4;
            }
        }

        QString model_shortname = parser.value("model");

        // Try to find our model in the list of models
        QString modelpath;
        for (auto&& model : models_.getInstalledModels()) {
            if (model.shortName == model_shortname) {
                modelpath = model.path;
            }
        }
        if (modelpath.isEmpty()) {
            qCritical() << "We could not find a model identified as:" << model_shortname << ". Use translateLocally -l to list available models or use the GUI to download some from the internet.";
            return 1;
        }

        // Init the translation model
        translator_->setModel(modelpath, settings_.marianSettings());
        doTranslation();
        return 0;
    } else {
        qCritical() << "We are in command line mode, but there's nothing for us to do. Some control flow mistake maybe?";
        return 2;
    }
}

void CommandLineIface::printLocalModels() {
    QList<Model> localmodels = models_.getInstalledModels();
    QTextStream out(stdout);
    for (auto&& model : localmodels) {
        out << model.src << "-" << model.trg << " type: " << model.type << " version: " << model.localversion << "; To invoke do -m " << model.shortName << '\n';
    }
}

QString &CommandLineIface::fetchData(QString &buffer) {
    int counter = 0;
    buffer.clear();
    static QString line;
    
    while(counter < prefetchLines && instream_.readLineInto(&line)) {
        buffer.append(line);
        buffer.append('\n'); // The new line has no EoL characters
        counter++;
    }

    return buffer;
}

/* This function is pseudo blocking, via an event loop.*/
void CommandLineIface::doTranslation() {
    QString input;
    while (!fetchData(input).isEmpty()) {
        translator_->translate(input);
        // Start event loop to block unit translation is ready. Translator
        // will call outputTranslation or outputError during this call, which
        // will either unblock this exec() call, or kill the program.
        eventLoop_.exec();
    }
}

void CommandLineIface::outputError(QString error) {
    qCritical() << error;
    exit(22);
}

void CommandLineIface::outputTranslation(Translation output) {
    outstream_ << output.translation();
    outstream_.flush();
    eventLoop_.exit(); // Unblock the main thread
}

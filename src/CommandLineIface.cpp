#include "CommandLineIface.h"
#include <QFile>

CommandLineIface::CommandLineIface(QCommandLineParser& parser, QObject * parent) : QObject(parent), eventLoop_(this), parser_(parser), models_(this),
                                                                                   settings_(this), translator_(new MarianInterface(this)),
                                                                                   qcin_(stdin), qcout_(stdout), qcerr_(stderr) {
    // Initialise input and output streams files if necessary
    if (parser_.isSet("i")) {
        infile_.reset(new QFile(parser_.value("i")));
        if (infile_->open(QIODevice::ReadOnly)) {
            instream_.reset(new QTextStream(infile_.get()));
        } else {
            outputError(QString("Couldn't open input file: " + parser_.value("i")));
        }
    }

    if (parser_.isSet("o")) {
        outfile_.reset(new QFile(parser_.value("o")));
        if (outfile_->open(QIODevice::WriteOnly)) {
            outstream_.reset(new QTextStream(outfile_.get()));
        } else {
            outputError(QString("Couldn't open output file: " + parser_.value("o")));
        }
    }

    // Take care of slots and signals
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

inline QString CommandLineIface::fetchData() {
    // Fetch up to prefetchLines number of lines from input
    QString ret("");
    int counter = 0;
    if (parser_.isSet("i")) {
        while(counter < prefetchLines && !instream_->atEnd()) {
            ret = ret + instream_->readLine();
            counter++;
        }
    } else {
        while (counter < prefetchLines && !qcin_.atEnd()) {
            ret = ret + qcin_.readLine();
            counter++;
        }
    }
    return ret;
}

/* This function is pseudo blocking, via an event loop.*/
void CommandLineIface::doTranslation() {
    // Find whether input is stdin or a file
    QString input;
    if (parser_.isSet("i")) {
        input = instream_->readAll(); // @TODO this should be more complicated batching thingie, but alas. Should work fine with most small inputs.
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
    qcerr_.flush();
    exit(22);
}

void CommandLineIface::outputTranslation(Translation output) {
    // Find whether output is stdin or a file
    if (parser_.isSet("o")) {
        *outstream_.get() << output.translation();
        outfile_->flush();
    } else {
        qcout_ << output.translation();
        qcout_.flush();
    }
    eventLoop_.exit(); // Unblock the main thread
}

CommandLineIface::~CommandLineIface() {
    if (parser_.isSet("i")) {
        infile_->close();
    }
    if (parser_.isSet("o")) {
        outfile_->close();
    }
}

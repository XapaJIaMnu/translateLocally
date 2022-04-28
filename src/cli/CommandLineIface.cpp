#include "CommandLineIface.h"
#include <QFile>
#include <QProcessEnvironment>

#include <array>

// Progress bar taken from https://stackoverflow.com/questions/14539867/how-to-display-a-progress-indicator-in-pure-c-c-cout-printf
#define PBSTR "############################################################"
#define PBWIDTH 60

namespace {
    void checkAppleSandbox(QCommandLineParser const &parser) {
        QProcessEnvironment env(QProcessEnvironment::systemEnvironment());
        if (!env.contains("APP_SANDBOX_CONTAINER_ID"))
            return;

        QString command("translateLocally");

        if (parser.isSet("m"))
            command += QString(" -m %1").arg(parser.value("m"));

        if (parser.isSet("i"))
            command += QString(" < \"%1\"").arg(parser.value("i"));

        if (parser.isSet("o"))
            command += QString(" > \"%1\"").arg(parser.value("o"));

        qCritical().noquote().nospace() << "Hint: "
          << "Files might not be accessible by translateLocally because it is running inside Apple's sandbox. "
          << "Try piping the file into translateLocally:\n"
          << "\n  " << command << "\n";
    }
}

CommandLineIface::CommandLineIface(QObject * parent)
: QObject(parent)
, eventLoop_(this)
, network_(this)
, settings_(this)
, models_(this, &settings_)
, translator_(new MarianInterface(this))
, instream_(stdin)
, outstream_(stdout) {
    // Take care of slots and signals
    connect(translator_, &MarianInterface::error, this, &CommandLineIface::outputError);
    connect(translator_, &MarianInterface::translationReady, this, &CommandLineIface::outputTranslation);
    connect(&network_, &Network::error, this, &CommandLineIface::outputError);
}

int CommandLineIface::run(QCommandLineParser const &parser) {
    if (parser.isSet("l")) {
        printLocalModels();
        return 0;
    } else if (parser.isSet("a")) {
        connect(&models_, &ModelManager::fetchedRemoteModels, this, &CommandLineIface::printRemoteModels);
        models_.fetchRemoteModels();
        eventLoop_.exec(); // Network operations take some time, therefore we need to wait for the remote models to be fetched and then exit
        return 0;
    } else if (parser.isSet("d")) {
        downloadRemoteModel(parser.value("d"));
        return 0;
    } else if (parser.isSet("r")) {
        QString errorstr("Unable to find \'" + parser.value("r") + "\' in the list of available models. Available models:\n");
        QString successstr;
        bool deleted = false;
        bool found = false;
        for (auto && model : models_.getInstalledModels()) {
            errorstr = errorstr + model.src + "-" + model.trg + " type " + model.type + "; To remove do -r " + model.shortName + '\n';
            if (model.shortName == parser.value("r")) {
                successstr = "Model " + model.src + "-" + model.trg + " type " + model.type + " successfully removed.\n";
                deleted = models_.removeModel(model);
                found = true;
            }
        }
        if (!found) {
            qCritical().noquote() << errorstr;
            return 6;
        }
        if (!deleted) {
            qCritical().noquote() << "Unable to remove model " << parser.isSet("r") << " from the local machine. Perhaps it is not managed by translateLocally?";
            return 7;
        }
        QTextStream out(stdout);
        out << successstr;
        out.flush();
        return 0;
    } else if (parser.isSet("m")) {
        // Open file as input stream if necessary
        if (parser.isSet("i")) {
            infile_.setFileName(parser.value("i"));
            if (infile_.open(QIODevice::ReadOnly)) {
                instream_.setDevice(&infile_);
            } else {
                checkAppleSandbox(parser);
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
                checkAppleSandbox(parser);
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
    QTextStream out(stdout);
    for (auto&& model : models_.getInstalledModels()) {
        out << model.src << "-" << model.trg << " type: " << model.type << " version: " << model.localversion << "; To invoke do -m " << model.shortName << '\n';
    }
}

/**
 * @brief Prints remote models and exits. it also takes care of an event loop because the network operation could take some time.
 *        This function is pseudo blocking, via an event loop.
 */
void CommandLineIface::printRemoteModels() {
    QTextStream out(stdout);
    for (auto&& model : models_.getNewModels()) {
        out << model.src << "-" << model.trg << " type: " << model.type << " version: " << model.remoteversion << "; To download do -d " << model.shortName << '\n';
    }
    for (auto&& model : models_.getUpdatedModels()) {
        out << model.src << "-" << model.trg << " type: " << model.type << " version: " << model.remoteversion << " localVersion: " << model.localversion <<
               "; To download do -d " << model.shortName << '\n';
    }
    eventLoop_.exit();
}

/**
 * @brief CommandLineIface::fetchData fetches lines to be translated, batches them for efficiency and sends to the translator. Could be either a file or stdin
 * @param buffer the buffer is where the lines to be translated are stored
 * @return
 */
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
/**
 * @brief CommandLineIface::doTranslation This function is pseudo blocking, via an event loop. It sends text to be translated by marian.
 */
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

void CommandLineIface::downloadRemoteModel(QString modelID) {
    // fetch model from the internet and wait until it is there
    connect(&models_, &ModelManager::fetchedRemoteModels, this, [&](){eventLoop_.exit();});
    models_.fetchRemoteModels();
    eventLoop_.exec();

    // identify the model we want to download. This is a remote model and we have this complicated lambda function
    // because i wanted it to be a const & as opposed to a copy of a model
    const Model& model = [&]() {
        // Keep track of available models in case we have an error
        QString errorstr("Unable to find \'" + modelID + "\' in the list of available models. Available models:\n");
        for (const Model& model : models_.getRemoteModels()) {
            errorstr = errorstr + model.src + "-" + model.trg + " type: " + model.type + "; To download do -d " + model.shortName + '\n';
            if (model.shortName == modelID) {
                return model;
            }
        }
        outputError(errorstr);
        return models_.getRemoteModels().at(0); // This line will never be executed, since we actually exit in the case of an error
    }();
    // Set up progress bar for downloading:
    QTextStream out(stdout);
    out << "Downloading " << model.src << "-" << model.trg << " type " << model.type << "...\n";
    out.flush();
    connect(&network_, &Network::progressBar, this, [&](qint64 ist, qint64 max) {
        // taken from https://stackoverflow.com/questions/14539867/how-to-display-a-progress-indicator-in-pure-c-c-cout-printf
        double percentage = (double)ist/(double)max;
        int val = (int) (percentage * 100);
        int lpad = (int) (percentage * PBWIDTH);
        int rpad = PBWIDTH - lpad;
        printf("\r%3d%% [%.*s%*s]", val, lpad, PBSTR, rpad, "");
        fflush(stdout);
    });
    // Download the new model. Use eventloop again to prevent premature exit before download is finished
    connect(&network_, &Network::downloadComplete, this, [&](QFile *file, QString filename) {
        // We use cout here, as QTextStream out gives a warning about being lamda captured.
        std::cout << "\nModel downloaded successfully! You can now invoke it with -m " << modelID.toStdString() << std::endl;
        models_.writeModel(file, ModelMeta{}, filename); // TODO: ModelMeta
        eventLoop_.exit();
    });
    QNetworkReply *reply = network_.downloadFile(model.url, QCryptographicHash::Sha256, model.checksum);
    if (reply == nullptr) {
        outputError("Could not connect to the internet and download: " + model.url);
    }
    eventLoop_.exec();
}

void CommandLineIface::outputError(QString error) {
    qCritical().noquote() << error;
    exit(22);
}

void CommandLineIface::outputTranslation(Translation output) {
    outstream_ << output.translation();
    outstream_.flush();
    eventLoop_.exit(); // Unblock the main thread
}

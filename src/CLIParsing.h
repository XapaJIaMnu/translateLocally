#pragma once
#include <QApplication>
#include <QCommandLineParser>

namespace translateLocally {

/**
 * @brief CLIArgumentInit Inits the command line argument parsing.
 * @param translateLocallyApp The translateLocally QApplicaiton
 * @param parser The command line parser.
 */

static void CLIArgumentInit(QApplication& translateLocallyApp, QCommandLineParser& parser) {
    parser.setApplicationDescription("A secure translation service that performs translations for you locally, on your own machine.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({{"l", "list-models"}, QObject::tr("List locally installed models.")});
    parser.addOption({{"a", "available-models"}, QObject::tr("Connect to the Internet and list available models. Only shows models that are NOT installed locally or have a new version available online.")});
    parser.addOption({{"d", "download-model"}, QObject::tr("Connect to the Internet and download a model."), "output", ""});
    parser.addOption({{"r", "remove-model"}, QObject::tr("Remove a model from the local machine. Only works for models managed with translateLocally."), "output", ""});
    parser.addOption({{"m", "model"}, QObject::tr("Select model for translation."), "model", ""});
    parser.addOption({{"i", "input"}, QObject::tr("Source translation file (or just used stdin)."), "input", ""});
    parser.addOption({{"o", "output"}, QObject::tr("Target translation file (or just used stdout)."), "output", ""});
    parser.process(translateLocallyApp);
}

/**
 * @brief isCLIOnly Checks if the application should run in cliONLY mode or launch the GUI
 * @param parser The command line parser.
 * @return
 */

static bool isCLIOnly(QCommandLineParser& parser) {
    QList<QString> cmdonlyflags = {"l", "a", "d", "r", "m", "i", "o"};
    bool cliONLY = false;
    for (auto&& flag : cmdonlyflags) {
        if (parser.isSet(flag)) {
            cliONLY = true;
            break;
        }
    }
    return cliONLY;
}

} // namespace translateLocally

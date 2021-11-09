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
    parser.addOption({{"l", "list-models"}, QObject::tr("List available models.")});
    parser.addOption({{"m", "model"}, QObject::tr("Select model for translation."), "model", ""});
    parser.addOption({{"i", "input"}, QObject::tr("Source translation text (or just used stdin)"), "input", ""});
    parser.addOption({{"o", "output"}, QObject::tr("Target translation output (or just used stdout)"), "output", ""});
    parser.process(translateLocallyApp);
}

/**
 * @brief isCLIOnly Checks if the application should run in cliONLY mode or launch the GUI
 * @param parser The command line parser.
 * @return
 */

static bool isCLIOnly(QCommandLineParser& parser) {
    QList<QString> cmdonlyflags = {"m", "l", "i", "o"};
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

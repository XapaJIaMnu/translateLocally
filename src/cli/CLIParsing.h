#pragma once
#include "constants.h"
#include <QApplication>
#include <QCommandLineParser>

namespace translateLocally {

enum AppType {
    CLI,
    GUI,
    NativeMsg
};

/**
 * @brief CLIArgumentInit Inits the command line argument parsing.
 * @param translateLocallyApp The translateLocally QApplicaiton
 * @param parser The command line parser.
 */

template<class QAppType>
static void CLIArgumentInit(QAppType& translateLocallyApp, QCommandLineParser& parser) {
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
    parser.addOption({{"p", "plugin"}, QObject::tr("Start native message server to use for a browser plugin.")});
    parser.process(translateLocallyApp);
}

/**
 * @brief runType Checks whether to run in CLI, GUI or native message interface server
 * @param parser The command line parser.
 * @return the launched main type
 */

static AppType runType(QCommandLineParser& parser) {
    QList<QString> cmdonlyflags = {"l", "a", "d", "r", "m", "i", "o"};
    QList<QString> nativemsgflags = {"p"};
    for (auto&& flag : nativemsgflags) {
        if (parser.isSet(flag)) {
            return NativeMsg;
        }
    }

    for (auto&& flag : cmdonlyflags) {
        if (parser.isSet(flag)) {
            return CLI;
        }
    }

    // Search for the extension ID among the start-up arguments. This is the only thing
    // the native messaging APIs of Firefox and Chrome have in common. See also:
    // https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Native_messaging#extension_side
    for (auto&& path : parser.positionalArguments()) {
        if (kNativeMessagingClients.contains(path)) {
            return NativeMsg;
        }
    }

    return GUI;
}

} // namespace translateLocally

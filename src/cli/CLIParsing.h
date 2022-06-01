#pragma once
#include <QApplication>
#include <QCommandLineParser>
#include <QRegularExpression>
#include "../settings/Settings.h"

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
    parser.addOption({"allow-client", QObject::tr("Add a native messaging client id that is allowed to use Native Messaging in the browser.")});
    parser.addOption({"remove-client", QObject::tr("Remove a native messaging client id.")});
    parser.addOption({"list-clients", QObject::tr("List allowed native messaging clients")});
    parser.addOption({"update-manifests", QObject::tr("Register native messaging clients with user profile.")});
    parser.process(translateLocallyApp);
}

/**
 * @brief runType Checks whether to run in CLI, GUI or native message interface server
 * @param parser The command line parser.
 * @return the launched main type
 */

static AppType runType(QCommandLineParser& parser) {
    QList<QString> cmdonlyflags = {"l", "a", "d", "r", "m", "i", "o", "allow-client", "remove-client", "update-manifests", "list-clients"};
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

    Settings settings;

    QRegularExpression re("^chrome-extension://(.+?)/$");

    for (auto&& path : parser.positionalArguments()) {
        // Matches anything Firefox based
        if (settings.nativeMessagingClients().contains(path)) {
            return NativeMsg;
        }

        // Matching anything Chromium based
        auto match = re.match(path);
        if (match.hasMatch() && settings.nativeMessagingClients().contains(match.captured(1))) {
            return NativeMsg;
        }
    }

    // TODO: if this program is started as a browser's native messaging client,
    // but it is not registered (i.e. above code doesn't catch it) will this
    // cause the program to pop up with GUI and everything? Or will it just
    // properly error out? (This should never happen as the browser first looks
    // for allowed clients in the json file, but what if the json file is out
    // of sync with the settings?)

    return GUI;
}

} // namespace translateLocally

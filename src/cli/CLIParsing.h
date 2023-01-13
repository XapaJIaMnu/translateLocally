#pragma once
#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QRegularExpression>
#include "settings/Settings.h"

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
    parser.addOption({"debug", QObject::tr("Print debug messages")});
    parser.addOption({"html", QObject::tr("Input is HTML")});
    
    parser.process(translateLocallyApp);
}

/**
 * @bief Will get the native messaging ID from the command line args if there is one. If not, will return empty string.
 * @return client id or empty string
 */
static QString getNativeMessagingClientId() {
    // Look at the positional command line arguments to check whether we're being called from Firefox or Chromium.
    // See also:
    // https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Native_messaging#extension_side
    // Call site for Firefox, which passes path to manifest.json + extension id:
    // https://searchfox.org/mozilla-central/rev/bf6f194694c9d1ae92847f3d4e4c15c2486f3200/toolkit/components/extensions/NativeMessaging.jsm#101
    // Call site for Chrome, which just gives you extension origin with some additional args on Windows:
    // https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/extensions/api/messaging/native_process_launcher.cc;l=235-239;drc=83230694ec1e53e8e53458f502f0adf1eade0408
    auto const &args = QCoreApplication::arguments();

    // On Firefox, the first argument is path to a manifest file, and the second is the extension id. I'm intentionally
    // not checking for known full paths, just for the common suffix shared by all browser implementations, so people
    // with Firefox forks or weird Firefox installations can still use this functionality as long as they copy the
    // manifest file into the right folder by themselves.
    if (args.size() == 3 && args[1].endsWith(".json"))
        return args[2];

    // On Chrome, the first argument is the extension origin, which is chrome's internal url pattern for
    // anything related to a specific extension. Chrome can pass in additional
    // arguments on windows, so no specific check on size() here.
    QRegularExpression chromeOriginPattern("^chrome-extension://(.+?)/$");
    if (args.size() > 2) {
        auto match = chromeOriginPattern.match(args[1]);
        if (match.hasMatch())
            return match.captured(1);
    }

    return QString();
}

/**
 * @brief runType Checks whether to run in CLI, GUI or native message interface server
 * @param parser The command line parser.
 * @return the launched main type
 */

static AppType runType(QCommandLineParser& parser) {
    // Native messaging called from a browser
    QString nativeClientId = getNativeMessagingClientId();
    if (!nativeClientId.isEmpty()) {
        if (Settings().nativeMessagingClients().contains(nativeClientId)) {
            return NativeMsg;
        } else {
            qCritical() << "The command line args matched that of a browser trying to start a native messaging host, "
                           "but the extension id provided is not known to translateLocally. Did you register it using "
                           "`translateLocally --allow-client" << nativeClientId << "`?";
            exit(128); // Exiting because possibly security issue.
        }
    }

    // Manual native messaging mode through -p or --plugin flag
    if (parser.isSet("plugin")) {
        return NativeMsg;
    }

    // Cli mode
    QList<QString> cmdonlyflags = {"l", "a", "d", "r", "m", "i", "o", "allow-client", "remove-client", "update-manifests", "list-clients"};
    for (auto&& flag : cmdonlyflags) {
        if (parser.isSet(flag)) {
            return CLI;
        }
    }

    // Non-argument mode
    return GUI;
}

} // namespace translateLocally

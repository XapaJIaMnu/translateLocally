#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <memory>
#include <functional>
#include "MarianInterface.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QSaveFile>
#include <QDir>
#include <QMessageBox>
#include <QFontDialog>
#include <QListView>

// TODO: use Q_ENUM for this
const QString MainWindow::kActionFetchRemoteModels("FETCH_REMOTE_MODELS");

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui_(new Ui::MainWindow)
    , models_(this)
    , localModelDelegate_(this)
    , translatorSettingsDialog_(this, models_.getSettings())
    , network_(this)
{
    ui_->setupUi(this);

    // Hide download progress bar
    showDownloadPane(false);

    // Hide settings window
    translatorSettingsDialog_.setVisible(false);

    updateLocalModels();

    if (!models_.installedModels().empty())
        resetTranslator(models_.installedModels().first().path);

    inactivityTimer_.setInterval(300);
    inactivityTimer_.setSingleShot(true);
    
    // Attach slots
    connect(&models_, &ModelManager::error, this, &MainWindow::popupError); // All errors from the model class will be propagated to the GUI
    connect(&models_, &QAbstractTableModel::dataChanged, this, &MainWindow::updateLocalModels);
    connect(&models_, &QAbstractTableModel::rowsInserted, this, &MainWindow::updateLocalModels);
    connect(&models_, &QAbstractTableModel::rowsRemoved, this, &MainWindow::updateLocalModels);
    connect(&network_, &Network::error, this, &MainWindow::popupError); // All errors from the network class will be propagated to the GUI
    connect(&inactivityTimer_, &QTimer::timeout, this, &MainWindow::on_translateAction_triggered);
    connect(&translatorSettingsDialog_, &TranslatorSettingsDialog::settingsChanged, this, &MainWindow::updateModelSettings);
}

MainWindow::~MainWindow() {
    delete ui_;
}

void MainWindow::on_translateAction_triggered()
{
    translate();
}

void MainWindow::on_inputBox_textChanged() {
    QString inputText = ui_->inputBox->toPlainText();
    inactivityTimer_.stop();

    if (inputText.isEmpty())
        return;

    // Remove the last word, because it is likely incomplete
    auto lastSpace = inputText.lastIndexOf(" ");
    
    while (lastSpace >= 0 && inputText[lastSpace].isSpace())
        --lastSpace;

    if (lastSpace != -1)
        inputText.truncate(lastSpace);

    if (inputText != translationInput_) {
        translationInput_ = inputText;
        translate(inputText);
    }

    // Reset our "person stopped typing" timer
    inactivityTimer_.start();
}

void MainWindow::showDownloadPane(bool visible)
{
    ui_->downloadPane->setVisible(visible);
    ui_->localModels->setVisible(!visible);
}

void MainWindow::handleDownload(QString filename, QByteArray data) {
    models_.writeModel(filename, data);
}

void MainWindow::downloadProgress(qint64 ist, qint64 max) {
    ui_->downloadProgress->setRange(0,max);
    ui_->downloadProgress->setValue(ist);
}

/**
 * @brief MainWindow::on_localModels_activated Change the loaded translation model to something else.
 * @param index index of the selected item in the localModels combobox.
 */

void MainWindow::on_localModels_activated(int index) {
    QVariant data = ui_->localModels->itemData(index);

    if (data.canConvert<LocalModel>()) {
        resetTranslator(data.value<LocalModel>().path);
    } else if (data.canConvert<RemoteModel>()) {
        //@TODO check if model is already downloaded and prompt user for download confirmation
        connect(&network_, &Network::progressBar, this, &MainWindow::downloadProgress, Qt::UniqueConnection);
        connect(&network_, &Network::downloadComplete, this, &MainWindow::handleDownload, Qt::UniqueConnection);
        RemoteModel model = data.value<RemoteModel>();
        showDownloadPane(true);
        ui_->downloadLabel->setText(QString("Downloading %1…").arg(model.name));
        QNetworkReply *reply = network_.downloadFile(model.url);
        connect(ui_->cancelDownloadButton, &QPushButton::clicked, reply, &QNetworkReply::abort, Qt::UniqueConnection);
        connect(reply, &QNetworkReply::finished, this, [&]() {
            showDownloadPane(false);
        });
    } else if (data.userType() == QMetaType::QString && data.toString() == kActionFetchRemoteModels) {
        connect(&models_, &ModelManager::fetchedRemoteModels, this, [&]() {
            ui_->localModels->showPopup();
        });
        models_.fetchRemoteModels();
    } else {
        qDebug() << "Unknown option: " << data;
    }
}


void MainWindow::updateLocalModels() {
    // Clear out current items
    ui_->localModels->clear();

    // Add local models
    if (!models_.installedModels().empty()) {
        for (auto &&model : models_.installedModels())
            ui_->localModels->addItem(model.name, QVariant::fromValue(model));

        ui_->localModels->insertSeparator(ui_->localModels->count());
    }

    // Add any models available for download
    if (!models_.availableModels().empty()) {
        for (auto &&model : models_.availableModels())
            ui_->localModels->addItem(model.name, QVariant::fromValue(model));
    } else {
        ui_->localModels->addItem("Download models…", QVariant::fromValue(kActionFetchRemoteModels));
    }
}


void MainWindow::translate() {
    translate(ui_->inputBox->toPlainText());
}

void MainWindow::translate(QString const &text) {
    ui_->translateAction->setEnabled(false); //Disable the translate button before the translation finishes
    if (translator_) {
        if (!text.isEmpty()) {
            ui_->outputBox->setText("Translating, please wait...");
            this->repaint(); // Force update the UI before the translation starts so that it can show the previous text
            translator_->translate(text);
        } else {
            ui_->outputBox->setText("");
            ui_->translateAction->setEnabled(true);
        }
    } else {
        popupError("You need to download a translation model first. Do that with the interface on the right.");
    }    
}

/**
 * @brief MainWindow::resetTranslator Deletes the old translator object and creates a new one with the new language
 * @param dirname directory where the model is found
 */

void MainWindow::resetTranslator(QString dirname) {
    // Disconnect existing slots:
    if (translator_) {
        disconnect(translator_.get());
    }
    QString model0_path = dirname + "/";
    ui_->localModels->setEnabled(false); // Disable changing the model while changing the model
    ui_->translateAction->setEnabled(false); //Disable the translate button before the swap

    translator_.reset(); // Do this first to free the object.
    translator_.reset(new MarianInterface(model0_path, models_.getSettings() , this));

    ui_->translateAction->setEnabled(true); // Reenable it
    ui_->localModels->setEnabled(true); // Disable changing the model while changing the model

    // Set up the connection to the translator
    connect(translator_.get(), &MarianInterface::translationReady, this, [&](QString translation){ui_->outputBox->setText(translation);
                                                                                                  ui_->localModels->setEnabled(true); // Re-enable model changing
                                                                                                  ui_->translateAction->setEnabled(true); // Re-enable button after translation is done
                                                                                                 });

    translate();
}

/**
 * @brief MainWindow::popupError this will produce an error message from various subclasses
 * @param error the error message
 * NOTES: This message bug will occasionally trigger the harmless but annoying 4 year old bug https://bugreports.qt.io/browse/QTBUG-56893
 * This is basically some harmless console noise of the type: qt.qpa.xcb: QXcbConnection: XCB error: 3 (BadWindow
 */

void MainWindow::popupError(QString error) {
    QMessageBox msgBox(this);
    msgBox.setText(error);
    msgBox.exec();
}

void MainWindow::on_fontAction_triggered()
{
    this->setFont(QFontDialog::getFont(0, this->font()));
}

void MainWindow::on_actionTranslator_Settings_triggered() {
    this->translatorSettingsDialog_.setVisible(true);
}

/**
 * @brief MainWindow::updateModelSettings updates the memory and cores settings. Also resets the translation model with the new settings
 *                                        if the translation model is running. Usually called by a signal
 * @param memory new workspaces value
 * @param cores new thread count
 */

void MainWindow::updateModelSettings(size_t memory, size_t cores) {
    models_.getSettings().setWorkspace(memory);
    models_.getSettings().setCores(cores);
    if (translator_) {
        resetTranslator(translator_->mymodel);
    }
}


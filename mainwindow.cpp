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
#include <QWindow>
#include "logo/logo_svg.h"
#include <iostream>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui_(new Ui::MainWindow)
    , settings_(this)
    , models_(this)
    , localModelDelegate_(this)
    , translatorSettingsDialog_(this, &settings_)
    , network_(this)
    , translator_(new MarianInterface(this))
{
    ui_->setupUi(this);

    // Create icon for the main window
    QIcon icon = translateLocally::logo::getLogoFromSVG();
    this->setWindowIcon(icon);

    // Create the status bar
    ui_->statusbar->addPermanentWidget(ui_->pendingIndicator);
    ui_->pendingIndicator->hide();

    // Hide download progress bar
    showDownloadPane(false);

    // Hide settings window
    translatorSettingsDialog_.setVisible(false);

    updateLocalModels();

    // If no model is preferred, load the first available one.
    if (settings_.translationModel().isEmpty() && !models_.getInstalledModels().empty())
        settings_.translationModel.setValue(models_.getInstalledModels().first().path);

    inactivityTimer_.setInterval(300);
    inactivityTimer_.setSingleShot(true);
    
    // Attach slots
    connect(&models_, &ModelManager::error, this, &MainWindow::popupError); // All errors from the model class will be propagated to the GUI
    connect(&models_, &QAbstractTableModel::dataChanged, this, &MainWindow::updateLocalModels);
    connect(&models_, &QAbstractTableModel::rowsInserted, this, &MainWindow::updateLocalModels);
    connect(&models_, &QAbstractTableModel::rowsRemoved, this, &MainWindow::updateLocalModels);
    connect(&network_, &Network::error, this, &MainWindow::popupError); // All errors from the network class will be propagated to the GUI
    
    // Set up the connection to the translator
    connect(translator_.get(), &MarianInterface::pendingChanged, ui_->pendingIndicator, &QProgressBar::setVisible);
    connect(translator_.get(), &MarianInterface::error, this, &MainWindow::popupError);
    connect(translator_.get(), &MarianInterface::translationReady, this, [&](QString translation) {
        ui_->outputBox->setText(translation);
        ui_->translateAction->setEnabled(true); // Re-enable button after translation is done
        ui_->translateButton->setEnabled(true);
    });

    // Queue translation when user has stopped typing for a bit
    connect(&inactivityTimer_, &QTimer::timeout, this, [&] {
        if (settings_.translateImmediately())
            translate();
    });

    // Pop open the model list again when remote model list is available
    connect(&models_, &ModelManager::fetchedRemoteModels, this, [&] {
        if (!models_.getNewModels().empty())
            ui_->localModels->showPopup();
    });

    // Connect translate immediately toggle in both directions
    connect(ui_->actionTranslateImmediately, &QAction::toggled, &settings_.translateImmediately, &decltype(Settings::translateImmediately)::setValue);
    connect(&settings_.translateImmediately, &Setting::valueChanged, this, &MainWindow::updateTranslateImmediately);

    // Update selected model when model changes
    connect(&settings_.translationModel, &Setting::valueChanged, this, &MainWindow::updateSelectedModel);

    // Connect settings changes to reloading the model.
    connect(&settings_.cores, &Setting::valueChanged, this, &MainWindow::resetTranslator);
    connect(&settings_.workspace, &Setting::valueChanged, this, &MainWindow::resetTranslator);
    connect(&settings_.translationModel, &Setting::valueChanged, this, &MainWindow::resetTranslator);

    // Initial state sync between settings & UI
    // TODO: Does Qt5 not have some form of bi-directional data binding? Looks
    // like it's only available in QtQuick and starting Qt6 in C++.
    // Note: both are safe when no model is set.
    resetTranslator();
    updateSelectedModel();
    updateTranslateImmediately();
}

MainWindow::~MainWindow() {
    delete ui_;
}

void MainWindow::updateTranslateImmediately() {
    ui_->actionTranslateImmediately->setChecked(settings_.translateImmediately());
    ui_->translateButton->setVisible(!settings_.translateImmediately());
}

void MainWindow::on_translateAction_triggered() {
    translate();
}

void MainWindow::on_translateButton_clicked() {
    translate();
}

void MainWindow::on_inputBox_textChanged() {
    if (!settings_.translateImmediately())
        return;
    
    QString inputText = ui_->inputBox->toPlainText();
    inactivityTimer_.stop();

    // Remove the last word, because it is likely incomplete
    auto lastSpace = inputText.lastIndexOf(" ");
    
    while (lastSpace > 0 && inputText[lastSpace-1].isSpace())
        --lastSpace;

    if (lastSpace > 0)
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
    ui_->modelPane->setVisible(!visible);
}

void MainWindow::handleDownload(QString filename, QByteArray data) {
    Model model = models_.writeModel(filename, data);
    if (model.isLocal())
        settings_.translationModel.setValue(model.path);
}

void MainWindow::downloadProgress(qint64 ist, qint64 max) {
    ui_->downloadProgress->setRange(0,max);
    ui_->downloadProgress->setValue(ist);
}

void MainWindow::downloadModel(Model model) {
    connect(&network_, &Network::progressBar, this, &MainWindow::downloadProgress, Qt::UniqueConnection);
    connect(&network_, &Network::downloadComplete, this, &MainWindow::handleDownload, Qt::UniqueConnection);
    
    ui_->downloadLabel->setText(QString("Downloading %1…").arg(model.modelName));
    ui_->downloadProgress->setValue(0);
    showDownloadPane(true);

    QNetworkReply *reply = network_.downloadFile(model.url);
    connect(ui_->cancelDownloadButton, &QPushButton::clicked, reply, &QNetworkReply::abort, Qt::UniqueConnection);
    connect(reply, &QNetworkReply::finished, this, [&]() {
        // Hide it here instead of in handleDownload() because finished() also
        // triggers on abort.
        showDownloadPane(false);
    });
}

/**
 * @brief MainWindow::on_localModels_activated Change the loaded translation model to something else.
 * @param index index of the selected item in the localModels combobox.
 */

void MainWindow::on_localModels_activated(int index) {
    QVariant data = ui_->localModels->itemData(index);

    if (data.canConvert<Model>() && data.value<Model>().isLocal()) {
        settings_.translationModel.setValue(data.value<Model>().path);
    } else if (data.canConvert<Model>()) {
        downloadModel(data.value<Model>());
    } else if (data == Action::FetchRemoteModels) {
        models_.fetchRemoteModels();
    } else {
        qDebug() << "Unknown option: " << data;
    }
}


void MainWindow::updateLocalModels() {
    // Clear out current items
    ui_->localModels->clear();
    ui_->localModels->setCurrentIndex(-1);

    // Update the lists of available models:
    models_.updateAvailableModels();

    // Add local models
    if (!models_.getInstalledModels().empty()) {
        for (auto &&model : models_.getInstalledModels()) {
            QString updated = model.outdated() ? " oudated " : ""; // Write down that the model has been updated
            ui_->localModels->addItem(model.modelName + updated, QVariant::fromValue(model));
        }

        ui_->localModels->insertSeparator(ui_->localModels->count());
    }

    // Add any models available for download
    if (models_.getRemoteModels().empty()) {
        ui_->localModels->addItem("Download models…", Action::FetchRemoteModels);
    } else if (models_.getNewModels().empty()) {
        ui_->localModels->addItem("No other models available online");
    } else {
        for (auto &&model : models_.getNewModels())
            ui_->localModels->addItem(model.modelName, QVariant::fromValue(model));
    }

    // Add models that are existing but a new version is available online
    if (!models_.getUpdatedModels().empty()) {
        ui_->localModels->insertSeparator(ui_->localModels->count());
        for (auto&& model : models_.getUpdatedModels()) {
            ui_->localModels->addItem(model.modelName + " updated ", QVariant::fromValue(model));
        }
    }
}

void MainWindow::updateSelectedModel() {
    for (int i = 0; i < ui_->localModels->count(); ++i) {
        QVariant item = ui_->localModels->itemData(i);
        if (item.canConvert<Model>() && item.value<Model>().path == settings_.translationModel()) {
            ui_->localModels->setCurrentIndex(i);
            return;
        }
    }

    // Model not found? Don't select any option at all.
    ui_->localModels->setCurrentIndex(-1);
}


void MainWindow::translate() {
    translate(ui_->inputBox->toPlainText());
}

void MainWindow::translate(QString const &text) {
    ui_->translateAction->setEnabled(false); //Disable the translate button before the translation finishes
    ui_->translateButton->setEnabled(false);
    if (translator_->model().isEmpty()) {
        popupError("You need to download a translation model first. Do that through the drop down menu on top.");
    } else {
        translator_->translate(text);
    }    
}

void MainWindow::resetTranslator() {
    // Don't do anything if there is no model selected.
    if (settings_.translationModel().isEmpty())
        return;

    translator_->setModel(settings_.translationModel(), settings_.marianSettings());
    
    // Schedule re-translation immediately if we're in automatic mode.
    if (settings_.translateImmediately())
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

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
#include <QStandardItem>
#include <QWindow>
#include "logo/logo_svg.h"
#include <iostream>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui_(new Ui::MainWindow)
    , settings_(this)
    , models_(this)
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

    // Attach slots
    connect(&models_, &ModelManager::error, this, &MainWindow::popupError); // All errors from the model class will be propagated to the GUI
    connect(&models_, &ModelManager::localModelsChanged, this, &MainWindow::updateLocalModels);
    connect(&network_, &Network::error, this, &MainWindow::popupError); // All errors from the network class will be propagated to the GUI
    connect(&network_, &Network::progressBar, this, &MainWindow::downloadProgress);
    connect(&network_, &Network::downloadComplete, this, &MainWindow::handleDownload);
    
    
    // Set up the connection to the translator
    connect(translator_.get(), &MarianInterface::pendingChanged, ui_->pendingIndicator, &QProgressBar::setVisible);
    connect(translator_.get(), &MarianInterface::error, this, &MainWindow::popupError);
    connect(translator_.get(), &MarianInterface::translationReady, this, [&](QString translation, int speed) {
        ui_->outputBox->setText(translation);
        ui_->translateAction->setEnabled(true); // Re-enable button after translation is done
        ui_->translateButton->setEnabled(true);
        if (speed > 0) { // Display the translation speed only if it's > 0. This prevents the user seeing weird number if pressed translate with empty input
            ui_->statusbar->showMessage(tr("Translation speed: %1 words per second.").arg(speed));
        } else {
            ui_->statusbar->clearMessage();
        }
    });

    // Pop open the model list again when remote model list is available
    connect(&models_, &ModelManager::fetchedRemoteModels, this, [&] {
        if (!models_.getNewModels().empty())
            ui_->localModels->showPopup();
    });

    // Connect translate immediately toggle in both directions
    connect(ui_->actionTranslateImmediately, &QAction::toggled, std::bind(&decltype(Settings::translateImmediately)::setValue, &settings_.translateImmediately, std::placeholders::_1, Setting::EmitWhenChanged));
    bind(settings_.translateImmediately, [&](bool enabled) {
        ui_->actionTranslateImmediately->setChecked(enabled);
        ui_->translateButton->setVisible(!enabled);
    });

    // Connect changing split orientation
    bind(settings_.splitOrientation, [&](Qt::Orientation orientation) {
        ui_->splitter->setOrientation(orientation);
        ui_->actionSplit_Horizontally->setChecked(orientation == Qt::Horizontal);
        ui_->actionSplit_Vertically->setChecked(orientation == Qt::Vertical);
    });

    // Restore window size
    restoreGeometry(settings_.windowGeometry());

    // Update selected model when model changes
    bind(settings_.translationModel, [&](QString path) {
        Q_UNUSED(path);
        updateSelectedModel();
    });

    // Connect settings changes to reloading the model.
    connect(&settings_.cores, &Setting::valueChanged, this, &MainWindow::resetTranslator);
    connect(&settings_.workspace, &Setting::valueChanged, this, &MainWindow::resetTranslator);

    // Connect model changes to reloading model and trigger initial loading of model
    bind(settings_.translationModel, std::bind(&MainWindow::resetTranslator, this));
}

MainWindow::~MainWindow() {
    settings_.windowGeometry.setValue(saveGeometry());
    delete ui_;
}

void MainWindow::on_translateAction_triggered() {
    translate();
}

void MainWindow::on_translateButton_clicked() {
    translate();
}

void MainWindow::on_inputBox_textChanged() {
    if (settings_.translateImmediately())
        translate();
}

void MainWindow::showDownloadPane(bool visible)
{
    ui_->downloadPane->setVisible(visible);
    ui_->modelPane->setVisible(!visible);
}

void MainWindow::handleDownload(QFile *file, QString filename) {
    Model model = models_.writeModel(file, filename);
    if (model.isLocal()) // if writeModel fails, model will be empty (and not local)
        settings_.translationModel.setValue(model.path, Setting::AlwaysEmit);
}

void MainWindow::downloadProgress(qint64 ist, qint64 max) {
    ui_->downloadProgress->setRange(0,max);
    ui_->downloadProgress->setValue(ist);
}

void MainWindow::downloadModel(Model model) {
    ui_->downloadLabel->setText(tr("Downloading %1…").arg(model.modelName));
    ui_->downloadProgress->setValue(0);
    ui_->cancelDownloadButton->setEnabled(true);
    showDownloadPane(true);

    QNetworkReply *reply = network_.downloadFile(model.url);
    connect(ui_->cancelDownloadButton, &QPushButton::clicked, reply, &QNetworkReply::abort);

    connect(reply, &QNetworkReply::finished, this, [&]() {
        // When aborting, the downloading model is still selected. Reset it to
        // the currently loaded model.
        // TODO: This might show the wrong model briefly during extraction
        updateSelectedModel();

        // Switch back to the input model pane
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

    // Add local models
    if (!models_.getInstalledModels().empty()) {
        for (auto &&model : models_.getInstalledModels()) {
            // Write down that the model has been updated
            QString format = model.outdated() ? tr("%1 (outdated)") : QString("%1");
            ui_->localModels->addItem(format.arg(model.modelName), QVariant::fromValue(model));
        }
    } else {
        // Add a placeholder item
        ui_->localModels->addItem(tr("Press here to get started."));
        // ... but disable it for good measure
        dynamic_cast<QStandardItemModel*>(ui_->localModels->model())->item(0, 0)->setEnabled(false);
    }

    // Add any models available for download
    ui_->localModels->insertSeparator(ui_->localModels->count());
    if (models_.getRemoteModels().empty()) {
        ui_->localModels->addItem(tr("Download models…"), Action::FetchRemoteModels);
    } else if (models_.getNewModels().empty()) {
        ui_->localModels->addItem(tr("No other models available online"));
    } else {
        for (auto &&model : models_.getNewModels())
            ui_->localModels->addItem(model.modelName, QVariant::fromValue(model));
    }

    // Add models that are existing but a new version is available online
    if (!models_.getUpdatedModels().empty()) {
        ui_->localModels->insertSeparator(ui_->localModels->count());
        for (auto&& model : models_.getUpdatedModels())
            ui_->localModels->addItem(tr("%1 (update available)").arg(model.modelName), QVariant::fromValue(model));
    }

    // Re-select the currently loaded model
    updateSelectedModel();
}

void MainWindow::updateSelectedModel() {
    // Local models empty? Select the "hint" option that's always the first
    if (models_.getInstalledModels().isEmpty()) {
        ui_->localModels->setCurrentIndex(0);
        return;
    }

    // Normal behaviour: find the item that matches the local model
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
        popupError(tr("You need to download a translation model first. Do that through the drop down menu on top."));
    } else {
        translator_->translate(text);
    }    
}

void MainWindow::resetTranslator() {
    // Don't do anything if there is no model selected.
    if (settings_.translationModel().isEmpty())
        return;

    // Don't do anything if the path given isn't valid (e.g. user deleted
    // model from disk but it is still mentioned in Settings)
    if (!QDir(settings_.translationModel()).exists())
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

void MainWindow::on_fontAction_triggered() {
    this->setFont(QFontDialog::getFont(0, this->font()));
}

void MainWindow::on_actionTranslator_Settings_triggered() {
    this->translatorSettingsDialog_.setVisible(true);
}

void MainWindow::on_actionSplit_Horizontally_triggered() {
    settings_.splitOrientation.setValue(Qt::Horizontal);
}

void MainWindow::on_actionSplit_Vertically_triggered() {
    settings_.splitOrientation.setValue(Qt::Vertical);
}
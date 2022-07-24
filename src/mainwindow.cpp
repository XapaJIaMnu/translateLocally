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
#include <QSignalBlocker>
#include <QStandardItem>
#include <QStandardPaths>
#include <QWindow>
#include "Translation.h"
#include "cli/NativeMsgManager.h"
#include "logo/logo_svg.h"
#include <iostream>
#include <QScrollBar>
#include <QMessageBox>

namespace {
    void addDisabledItem(QComboBox *combobox, QString label) {
        combobox->addItem(label);
        dynamic_cast<QStandardItemModel*>(combobox->model())->item(combobox->count() - 1, 0)->setEnabled(false);
    }

    auto copyScrollPosition(QAbstractScrollArea *inputBox, QAbstractScrollArea *outputBox) {
        int value = inputBox->verticalScrollBar()->value();
        float percentage = (float) value / inputBox->verticalScrollBar()->maximum();
        outputBox->verticalScrollBar()->setValue((int) (outputBox->verticalScrollBar()->maximum() * percentage));
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui_(new Ui::MainWindow)
    , settings_(this)
    , models_(this, &settings_)
    , translatorSettingsDialog_(this, &settings_, &models_)
    , network_(this)
    , translator_(new MarianInterface(this))
    , alignmentWorker_(new AlignmentWorker(this))
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

    // If we have preferred model, but it no longer exists on disk, reset it to empty
    if (!settings_.translationModel().isEmpty()) {
        auto model = models_.getModelForPath(settings_.translationModel());
        if (!model || !model->isLocal())
            settings_.translationModel.setValue("");
    }

    // If no model is preferred, load the first available one.
    if (settings_.translationModel().isEmpty() && !models_.getInstalledModels().empty())
        settings_.translationModel.setValue(models_.getInstalledModels().at(0).path);

    // Attach slots
    
    // As soon as we've started up, try to register this application as a native messaging host
    connect(this, &MainWindow::launched, this, &MainWindow::registerNativeMessagingAppManifest);

    connect(&models_, &ModelManager::error, this, &MainWindow::popupError); // All errors from the model class will be propagated to the GUI
    // Update when new models are discovered
    connect(&models_, &ModelManager::localModelsChanged, this, &MainWindow::updateLocalModels);
    // Update when the fetching remote model status changes
    connect(&models_, &ModelManager::fetchingRemoteModels, this, &MainWindow::updateLocalModels);
    connect(&models_, &ModelManager::fetchedRemoteModels, this, &MainWindow::updateLocalModels);
    // Make sure we unload any models that get deleted
    connect(&models_, &QAbstractItemModel::rowsAboutToBeRemoved, this, [&](const QModelIndex &parent, int first, int last) {
        Q_UNUSED(parent);
        // If no model is loaded right now, we don't need to worry.
        if (settings_.translationModel() == "")
            return;

        // For all removed rows, figure out which model they referred to and check whether that's
        // the currently loaded path. If it is, unload it.
        for (int i = first; i < last; ++i) {
            QVariant data = models_.data(models_.index(i, 0), Qt::UserRole);
            if (data.canConvert<Model>() && data.value<Model>().path == settings_.translationModel()) {
                settings_.translationModel.setValue("");
                break;
            }
        }
    });

    // Network is only used for downloading models
    connect(&network_, &Network::error, this, &MainWindow::popupError); // All errors from the network class will be propagated to the GUI
    connect(&network_, &Network::progressBar, this, &MainWindow::downloadProgress);
    connect(&network_, &Network::downloadComplete, this, &MainWindow::handleDownload);

    // Make downloading from the settings window.
    connect(&translatorSettingsDialog_, &TranslatorSettingsDialog::downloadModel, this, &MainWindow::downloadModelHelperSlot);
    
    // Set up the connection to the translator
    connect(translator_, &MarianInterface::pendingChanged, ui_->pendingIndicator, &QProgressBar::setVisible);
    connect(translator_, &MarianInterface::error, this, &MainWindow::popupError);
    connect(translator_, &MarianInterface::translationReady, this, [&](Translation translation) {
        translation_ = translation;
        
        {   
            // setPlainText() triggers a scrollpos reset to 0. We don't want
            // that, it looks really janky. So we block that signal, and then
            // manually resync the position afterwards.
            QSignalBlocker blocker(ui_->outputBox->verticalScrollBar());

            // We add a newline to the output to match the behaviour of the
            // input box which has an unreachable at the end of the text! You 
            // can't reach it with cursor keys, but it does show up when you use
            // the scrollbar. So to match the line count better, also add it to
            // the output.
            ui_->outputBox->setPlainText(translation_.translation() + QString("\n"));
        }

        // Restore scroll position after it jumped to 0 due to setPlainText.
        if (settings_.syncScrolling())
            ::copyScrollPosition(ui_->inputBox, ui_->outputBox);
        
        ui_->inputBox->document()->setModified(false); // Mark document as unmodified to tell highlighter alignment information is okay to use.
        ui_->translateAction->setEnabled(true); // Re-enable button after translation is done
        ui_->translateButton->setEnabled(true);
        if (translation_.wordsPerSecond() > 0) { // Display the translation speed only if it's > 0. This prevents the user seeing weird number if pressed translate with empty input
            ui_->statusbar->showMessage(tr("Translation speed: %1 words per second.").arg(translation_.wordsPerSecond()));
        } else {
            ui_->statusbar->clearMessage();
        }
    });

    connect(alignmentWorker_, &AlignmentWorker::ready, this, [&](QVector<WordAlignment> alignments, Translation::Direction direction) {
        if (!highlighter_)
            return;

        if (direction == Translation::source_to_translation) {
            QSignalBlocker blocker(ui_->outputBox); // block document change events caused by highlighter adding formatting
            highlighter_->setDocument(ui_->outputBox->document());
            highlighter_->highlight(alignments);    
        } else {
            QSignalBlocker blocker(ui_->inputBox);
            highlighter_->setDocument(ui_->inputBox->document());
            highlighter_->highlight(alignments);
        }
    });

    // Pop open the model list again when remote model list is available
    // Do that only if the settings window is not open as we want to avoid having the popup
    // override a potential error
    connect(&models_, &ModelManager::fetchedRemoteModels, this, [&] {
        if (!models_.getNewModels().empty() && !translatorSettingsDialog_.isVisible())
            ui_->localModels->showPopup();
    });

    // Connect translate immediately toggle in both directions
    connect(ui_->actionTranslateImmediately, &QAction::toggled, std::bind(&decltype(Settings::translateImmediately)::setValue, &settings_.translateImmediately, std::placeholders::_1, Setting::EmitWhenChanged));
    bind(settings_.translateImmediately, [&](bool enabled) {
        ui_->actionTranslateImmediately->setChecked(enabled);
        ui_->translateButton->setVisible(!enabled);
    });

    // Connect alignment highlight feature
    connect(ui_->actionShowAlignment, &QAction::toggled, std::bind(&decltype(Settings::showAlignment)::setValue, &settings_.showAlignment, std::placeholders::_1, Setting::EmitWhenChanged));
    bind(settings_.showAlignment, [&](bool enabled) {
        ui_->actionShowAlignment->setChecked(enabled);
        if (enabled) {
            QSignalBlocker blocker(ui_->outputBox);
            highlighter_ = new AlignmentHighlighter(this);
            highlighter_->setColor(settings_.alignmentColor());
            on_inputBox_cursorPositionChanged(); // trigger first highlight pass
        } else if (highlighter_) {
            highlighter_->deleteLater(); // Give it time to clean up old highlights
            highlighter_.clear(); // (note: deleteLater() would have done this as well, eventually)
        }
    });

    // Connect changing the highlight colour in settings to updating the highlighter to use it.
    connect(&settings_.alignmentColor, &Setting::valueChanged, this, [&](QString name, QVariant color) {
        if (highlighter_)
            highlighter_->setColor(color.value<QColor>());
        // if highlighter_ is not set right now, but will be created later on,
        // it will be initialised with the new colour.
    });

    // Connect synced scrolling toggle to setting
    connect(ui_->actionSyncScrolling, &QAction::toggled, std::bind(&decltype(Settings::syncScrolling)::setValue, &settings_.syncScrolling, std::placeholders::_1, Setting::EmitWhenChanged));
    bind(settings_.syncScrolling, [&](bool enabled) {
        ui_->actionSyncScrolling->setChecked(enabled);
        if (enabled)
            ::copyScrollPosition(ui_->inputBox, ui_->outputBox);
    });

    // Connect changing split orientation
    // TODO: we're not storing the position of the splitter, only orientation.
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

    // Connect translator setting changes to reloading the model.
    connect(&settings_.cores, &Setting::valueChanged, this, &MainWindow::resetTranslator);
    connect(&settings_.workspace, &Setting::valueChanged, this, &MainWindow::resetTranslator);

    // Connect model changes to reloading model and trigger initial loading of model
    bind(settings_.translationModel, std::bind(&MainWindow::resetTranslator, this));

    // When input box scrolls, scroll output box as well.
    connect(ui_->inputBox->verticalScrollBar(), &QAbstractSlider::valueChanged, this, [&]() {
        if (settings_.syncScrolling())
            ::copyScrollPosition(ui_->inputBox, ui_->outputBox);
    });

    // Oddly enough cursor movement doesn't trigger QAbstractSlider::valueChanged.
    // Note: Using Qt::QueuedConnection seems to make it less jumpy when you enter
    // newlines at the end in the input box. Maybe it gives the input
    // box more time to update its height and its scrollbar to update?
    connect(ui_->inputBox, &QPlainTextEdit::cursorPositionChanged, this, [&]() {
        if (settings_.syncScrolling())
            ::copyScrollPosition(ui_->inputBox, ui_->outputBox);
    }, Qt::QueuedConnection);

    emit launched();
}

void MainWindow::showEvent(QShowEvent *ev) {
    // Once everything is connected, put in first run dialog here
    QMainWindow::showEvent(ev);
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    QMetaObject::invokeMethod(this, "showFirstRunHelper", Qt::ConnectionType::QueuedConnection);
#else
    QMetaObject::invokeMethod(this, &MainWindow::showFirstRunHelper, Qt::ConnectionType::QueuedConnection);
#endif
}

void MainWindow::showFirstRunHelper() {
    if (models_.getInstalledModels().isEmpty()) {
        QMessageBox firstRun(QMessageBox::NoIcon, tr("First run"),
                               tr("Welcome to translateLocally, our privacy focussed machine translation system!\n\n\
It looks like you don't have any translation models currently installed.\n\n\
Would you like to connect to the Internet to see a list of available translation models?"),
                               QMessageBox::Ok | QMessageBox::Cancel, this);
        int ret = firstRun.exec();
        if (ret == QMessageBox::Ok) {
            models_.fetchRemoteModels();
        }
    }
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
    // If the localModels QComboBox will be visible, make sure it's accurate.
    if (!visible)
        updateSelectedModel();

    ui_->downloadPane->setVisible(visible);
    ui_->modelPane->setVisible(!visible);
}

void MainWindow::handleDownload(QFile *file, QString filename, QVariant extra) {
    ModelMeta meta = extra.value<ModelMeta>();
    meta.installedOn = QDateTime::currentDateTimeUtc();
    auto model = models_.writeModel(file, meta, filename);
    if (model) { // if writeModel didn't fail
        settings_.translationModel.setValue(model->path, Setting::AlwaysEmit);
    }
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

    // Pass on meta info about the model so we remember once we've downloaded it
    ModelMeta meta;
    meta.modelUrl = model.url;
    meta.repositoryUrl = model.repositoryUrl;

    qDebug() << "Downloading:" << model;

    QNetworkReply *reply = network_.downloadFile(model.url, QCryptographicHash::Sha256, model.checksum, QVariant::fromValue(meta));
    // If downloadFile could not create a temporary file, abort. network_ will
    // have emitted an error(QString) already so no need to notify.
    if (reply == nullptr) {
        showDownloadPane(false);
        return;
    }
    
    connect(ui_->cancelDownloadButton, &QPushButton::clicked, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::finished, this, [&]() {
        showDownloadPane(false);
    });
}

void MainWindow::downloadModelHelperSlot(Model model) {
    downloadModel(model);
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

    // Add local models at the top.
    if (!models_.getInstalledModels().empty()) {
        for (auto &&model : models_.getInstalledModels()) {
            // Write down that the model has been updated
            QString format = model.outdated() ? tr("%1 (outdated)") : QString("%1");
            ui_->localModels->addItem(format.arg(model.modelName), QVariant::fromValue(model));
        }
    } else {
        // Add a placeholder item to instruct users what to do next.
        addDisabledItem(ui_->localModels, tr("Press here to get started."));
    }

    // Next, add any models available for download that we don't already have locally
    ui_->localModels->insertSeparator(ui_->localModels->count());
    if (models_.getRemoteModels().empty()) {
        if (models_.isFetchingRemoteModels()) {
            addDisabledItem(ui_->localModels, tr("Downloading model list…"));
        } else {
            ui_->localModels->addItem(tr("Download models…"), Action::FetchRemoteModels);
        }
    } else if (models_.getNewModels().empty()) {
        ui_->localModels->addItem(tr("No other models available online"));
    } else {
        for (auto &&model : models_.getNewModels()) {
            auto repo = models_.getRepository(model);
            QString label = tr("%1 (from %2)").arg(model.modelName).arg(repo ? repo->name : model.repositoryUrl);
            ui_->localModels->addItem(label, QVariant::fromValue(model));
        }
        ui_->localModels->insertSeparator(ui_->localModels->count()); //@TODO some indication when no new models were fetched.
        ui_->localModels->addItem(tr("Download models…"), Action::FetchRemoteModels);
    }

    // Finally, add models that are existing but a new version is available online.
    // Note that you can still select the (outdated) model at the top.
    // @TODO should these be at the bottom, or between the local and remote-only models?
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
    if (settings_.translationModel() != "") {
        for (int i = 0; i < ui_->localModels->count(); ++i) {
            QVariant item = ui_->localModels->itemData(i);
            if (item.canConvert<Model>() && item.value<Model>().path == settings_.translationModel()) {
                ui_->localModels->setCurrentIndex(i);
                return;
            }
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
        if (models_.getInstalledModels().isEmpty()) {
            showFirstRunHelper();
        } else { // TBH I am not sure how we can ever end up in the else branch but better to have it.
            popupError(tr("You need to pick a translation model first. You can do that through the drop down menu on top."));
            ui_->localModels->showPopup(); // Makes it a bit more intuitive for the user to know what to do
        }
    } else {
        translator_->translate(text);
    }    
}

void MainWindow::resetTranslator() {
    // Note: settings_.translationModel() can be empty string, meaning unload the current model
    translator_->setModel(settings_.translationModel(), settings_.marianSettings());
    
    // Schedule re-translation immediately if we're in automatic mode.
    if (!settings_.translationModel().isEmpty() && settings_.translateImmediately())
        translate();
}

/**
 * @brief MainWindow::popupError this will produce an error message from various subclasses
 * @param error the error message
 * NOTES: This message bug will occasionally trigger the harmless but annoying 4 year old bug https://bugreports.qt.io/browse/QTBUG-56893
 * This is basically some harmless console noise of the type: qt.qpa.xcb: QXcbConnection: XCB error: 3 (BadWindow
 */

void MainWindow::popupError(QString error) {
    QMessageBox::critical(this, tr("An error occurred"), error);
}

void MainWindow::popupWarning(QString warning) {
    QMessageBox::warning(this, tr("Warning"), warning);
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

void MainWindow::on_inputBox_cursorPositionChanged() {
    if (!translation_ || !highlighter_)
        return;

    // Only show alignments when the document hasn't been modified since the
    // translation was made. Otherwise alignment information might be outdated
    // (i.e. offsets no longer match up with input text)
    if (!ui_->inputBox->document()->isModified()) {
        auto cursor = ui_->inputBox->textCursor();
        alignmentWorker_->query(translation_, Translation::source_to_translation, cursor.position(), cursor.anchor());
    } else {
        alignmentWorker_->query(Translation(), Translation::source_to_translation, 0, 0);
    }
}

void MainWindow::on_outputBox_cursorPositionChanged() {
    if (!translation_ || !highlighter_)
        return;

    // Ignore when it's not triggered by user interaction with this text box,
    // e.g when it is triggered by setPlainText() when translation is ready.
    if (!ui_->outputBox->hasFocus())
        return;

    // Only show alignments when the document hasn't been modified since the
    // translation was made. Otherwise alignment information might be outdated
    // (i.e. offsets no longer match up with input text)
    if (!ui_->outputBox->document()->isModified()) {
        auto cursor = ui_->outputBox->textCursor();
        alignmentWorker_->query(translation_, Translation::translation_to_source, cursor.position(), cursor.anchor());
    } else {
        alignmentWorker_->query(Translation(), Translation::translation_to_source, 0, 0);
    }
}

bool MainWindow::registerNativeMessagingAppManifest() {
    NativeMsgManager manager;
    return manager.writeNativeMessagingAppManifests(settings_.nativeMessagingClients());
}

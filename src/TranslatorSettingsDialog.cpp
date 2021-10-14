#include "TranslatorSettingsDialog.h"
#include "ui_TranslatorSettingsDialog.h"
#include <thread>
#include <QDesktopServices>
#include <QUrl>
#include <QFileDialog>
#include <QMessageBox>


TranslatorSettingsDialog::TranslatorSettingsDialog(QWidget *parent, Settings *settings, ModelManager *modelManager)
: QDialog(parent)
, ui_(new Ui::TranslatorSettingsDialog())
, settings_(settings)
, modelManager_(modelManager)
{
    ui_->setupUi(this);
    
    // Create lists of memory and cores
    QList<unsigned int> memory_options = {64, 128, 256, 512, 768, 1024, 1280, 1536, 1762, 2048};

    QList<unsigned int> cores_options;
    size_t max_cores = std::thread::hardware_concurrency();
    
    for (size_t cores = max_cores; cores > 0; cores -= 2)
        cores_options.prepend(cores);
    
    cores_options.prepend(1);

    for (auto option : memory_options)
        ui_->memoryBox->addItem(QString("%1").arg(option), option);

    for (auto option : cores_options)
        ui_->coresBox->addItem(QString("%1").arg(option), option);

    ui_->localModelTable->setModel(modelManager_);
    ui_->localModelTable->horizontalHeader()->setSectionResizeMode(ModelManager::Column::Name, QHeaderView::Stretch);
    ui_->localModelTable->horizontalHeader()->setSectionResizeMode(ModelManager::Column::Version, QHeaderView::ResizeToContents);

    connect(ui_->localModelTable->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TranslatorSettingsDialog::updateModelActions);
    connect(ui_->buttonBox, &QDialogButtonBox::accepted, this, &TranslatorSettingsDialog::applySettings);
    connect(ui_->actionRevealModel, &QAction::triggered, this, &TranslatorSettingsDialog::revealSelectedModels);
    connect(ui_->actionDeleteModel, &QAction::triggered, this, &TranslatorSettingsDialog::deleteSelectedModels);
    connect(ui_->deleteModelButton, &QPushButton::clicked, this, &TranslatorSettingsDialog::deleteSelectedModels);
    connect(ui_->importModelButton, &QPushButton::clicked, this, &TranslatorSettingsDialog::importModels);

    // Update context menu state on start-up
    updateModelActions();
}

TranslatorSettingsDialog::~TranslatorSettingsDialog()
{
    delete ui_;
}

void TranslatorSettingsDialog::updateSettings()
{
    ui_->coresBox->setCurrentIndex(ui_->coresBox->findData(settings_->cores()));
    ui_->memoryBox->setCurrentIndex(ui_->memoryBox->findData(settings_->workspace()));
    ui_->translateImmediatelyCheckbox->setChecked(settings_->translateImmediately());
    ui_->showAligmentsCheckbox->setChecked(settings_->showAlignment());
    ui_->alignmentColorButton->setColor(settings_->alignmentColor());
}

void TranslatorSettingsDialog::applySettings()
{
    settings_->cores.setValue(ui_->coresBox->currentData().toUInt());
    settings_->workspace.setValue(ui_->memoryBox->currentData().toUInt());
    settings_->translateImmediately.setValue(ui_->translateImmediatelyCheckbox->isChecked());
    settings_->showAlignment.setValue(ui_->showAligmentsCheckbox->isChecked());
    settings_->alignmentColor.setValue(ui_->alignmentColorButton->color());
}

void TranslatorSettingsDialog::revealSelectedModels()
{
    for (auto index : ui_->localModelTable->selectionModel()->selectedIndexes()) {
        Model model = modelManager_->data(index, Qt::UserRole).value<Model>();
        QDesktopServices::openUrl(QUrl(QString("file://%1").arg(model.path), QUrl::TolerantMode));
    }
}

void TranslatorSettingsDialog::deleteSelectedModels()
{
    QList<Model> selection;
    for (auto index : ui_->localModelTable->selectionModel()->selectedRows(0)) {
        Model model = modelManager_->data(index, Qt::UserRole).value<Model>();
        if (modelManager_->isManagedModel(model))
            selection.append(model);
    }

    if (QMessageBox::question(this,
            tr("Delete models"),
            tr("Are you sure you want to delete %n language model(s)?", "", selection.size())
        ) != QMessageBox::Yes)
        return;

    for (auto &&model : selection)
        modelManager_->removeModel(model);
}

void TranslatorSettingsDialog::importModels() {
    QStringList paths = QFileDialog::getOpenFileNames(this,
        tr("Open Translation model"),
        QString(),
        tr("Packaged translation model (*.tar.gz)"));

    for (QString const &path : paths) {
        QFile file(path);
        modelManager_->writeModel(&file);
    }
}

void TranslatorSettingsDialog::showEvent(QShowEvent *ev)
{
    // When this dialog pops up, update all widgets to reflect the current
    // settings.
    updateSettings();
}

void TranslatorSettingsDialog::updateModelActions()
{
    bool containsLocalModel = false;
    bool containsDeletableModel = false;

    for (auto index : ui_->localModelTable->selectionModel()->selectedIndexes()) {
        Model model = modelManager_->data(index, Qt::UserRole).value<Model>();
        
        if (model.isLocal())
            containsLocalModel = true;

        if (modelManager_->isManagedModel(model))
            containsDeletableModel = true;
    }

    ui_->actionRevealModel->setEnabled(containsLocalModel);
    ui_->actionDeleteModel->setEnabled(containsDeletableModel);
    ui_->deleteModelButton->setEnabled(containsDeletableModel);
}

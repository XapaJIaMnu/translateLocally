#include "TranslatorSettingsDialog.h"
#include "ui_TranslatorSettingsDialog.h"
#include <thread>
#include <QDesktopServices>
#include <QFileDialog>

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
}

TranslatorSettingsDialog::~TranslatorSettingsDialog()
{
    delete ui_;
}

void TranslatorSettingsDialog::showEvent(QShowEvent *ev)
{
    ui_->coresBox->setCurrentIndex(ui_->coresBox->findData(settings_->cores()));
    ui_->memoryBox->setCurrentIndex(ui_->memoryBox->findData(settings_->workspace()));
    ui_->translateImmediatelyCheckbox->setChecked(settings_->translateImmediately());
}

void TranslatorSettingsDialog::on_buttonBox_accepted()
{
    settings_->cores.setValue(ui_->coresBox->currentData().toUInt());
    settings_->workspace.setValue(ui_->memoryBox->currentData().toUInt());
    settings_->translateImmediately.setValue(ui_->translateImmediatelyCheckbox->isChecked());
}


void TranslatorSettingsDialog::on_actionRevealModel_triggered()
{
    for (auto index : ui_->localModelTable->selectionModel()->selectedIndexes()) {
        Model model = modelManager_->data(index, Qt::UserRole).value<Model>();
        QDesktopServices::openUrl(QUrl(QString("file://%1").arg(model.path), QUrl::TolerantMode));
    }
}

void TranslatorSettingsDialog::on_actionDeleteModel_triggered()
{
    for (auto index : ui_->localModelTable->selectionModel()->selectedIndexes()) {
        Model model = modelManager_->data(index, Qt::UserRole).value<Model>();
        modelManager_->removeModel(model);
    }
}

void TranslatorSettingsDialog::on_importModelButton_pressed() {
    QStringList paths = QFileDialog::getOpenFileNames(this,
        tr("Open Translation model"),
        QString(),
        tr("Packaged translation model (*.tar.gz)"));

    for (QString const &path : paths) {
        QFile file = QFile(path);
        modelManager_->writeModel(&file);
    }
}

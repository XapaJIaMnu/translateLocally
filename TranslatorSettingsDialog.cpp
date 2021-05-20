#include "TranslatorSettingsDialog.h"
#include "ui_TranslatorSettingsDialog.h"
#include <thread>

TranslatorSettingsDialog::TranslatorSettingsDialog(QWidget *parent, Settings *settings)
: QDialog(parent)
, ui_(new Ui::TranslatorSettingsDialog())
, settings_(settings)
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

    // TODO: load this every appearance
    ui_->coresBox->setCurrentIndex(cores_options.indexOf(settings_->cores()));
    ui_->memoryBox->setCurrentIndex(memory_options.indexOf(settings_->workspace()));
}

TranslatorSettingsDialog::~TranslatorSettingsDialog()
{
    delete ui_;
}

void TranslatorSettingsDialog::on_buttonBox_accepted()
{
    settings_->cores.setValue(ui_->coresBox->currentData().toUInt());
    settings_->workspace.setValue(ui_->memoryBox->currentData().toUInt());
}

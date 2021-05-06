#include "TranslatorSettingsDialog.h"
#include "ui_TranslatorSettingsDialog.h"

TranslatorSettingsDialog::TranslatorSettingsDialog(QWidget *parent, translateLocally::marianSettings& settings) :
    QDialog(parent),
    ui(new Ui::TranslatorSettingsDialog),
    settings_(settings)
{
    ui->setupUi(this);
    // Create lists of memory and cores
    memory_ = {64, 128, 256, 512, 768, 1024, 1280, 1536, 1762, 2048};
    memoryStr_ = QStringList{"64", "128", "256", "512", "768", "1024", "1280", "1536", "1762", "2048"};
    size_t max_cores = std::thread::hardware_concurrency();
    for (size_t cores = max_cores; cores > 0; cores -= 2) {
        cores_.prepend(cores);
        coresStr_.prepend(QString::fromStdString(std::to_string(cores)));
    }
    cores_.prepend(1);
    coresStr_.prepend(QString::fromStdString(std::to_string(1)));
    this->ui->coresBox->addItems(coresStr_);
    this->ui->memoryBox->addItems(memoryStr_);
    this->ui->coresBox->setCurrentIndex(cores_.indexOf(settings_.getCores()));
    this->ui->memoryBox->setCurrentIndex(memory_.indexOf(settings_.getWorkspace()));
}

TranslatorSettingsDialog::~TranslatorSettingsDialog()
{
    delete ui;
}

void TranslatorSettingsDialog::on_buttonBox_accepted()
{
    emit settingsChanged(memory_[this->ui->memoryBox->currentIndex()], cores_[this->ui->coresBox->currentIndex()]);
}

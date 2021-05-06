#ifndef TRANSLATORSETTINGS_H
#define TRANSLATORSETTINGS_H

#include <QDialog>
#include "types.h"

namespace Ui {
class TranslatorSettingsDialog;
}

class TranslatorSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TranslatorSettingsDialog(QWidget *parent, translateLocally::marianSettings& settings);
    ~TranslatorSettingsDialog();

private slots:
    void on_buttonBox_accepted();

signals:
    void settingsChanged(size_t memory, size_t cores);

private:
    Ui::TranslatorSettingsDialog *ui;
    const translateLocally::marianSettings& settings_;
    QList<size_t> cores_;
    QStringList coresStr_;
    QList<size_t> memory_;
    QStringList memoryStr_;
};

#endif // TRANSLATORSETTINGS_H

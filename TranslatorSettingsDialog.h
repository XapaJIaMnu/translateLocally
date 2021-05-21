#ifndef TRANSLATORSETTINGS_H
#define TRANSLATORSETTINGS_H

#include <QDialog>
#include "Settings.h"

namespace Ui {
class TranslatorSettingsDialog;
}

class TranslatorSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TranslatorSettingsDialog(QWidget *parent, Settings *settings);
    ~TranslatorSettingsDialog();

protected:
    void showEvent(QShowEvent *ev);

private slots:
    void on_buttonBox_accepted();

private:
    Ui::TranslatorSettingsDialog *ui_;
    Settings *settings_;
};

#endif // TRANSLATORSETTINGS_H

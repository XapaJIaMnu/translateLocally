#ifndef TRANSLATORSETTINGS_H
#define TRANSLATORSETTINGS_H

#include <QDialog>
#include "Settings.h"
#include "ModelManager.h"

namespace Ui {
class TranslatorSettingsDialog;
}

class TranslatorSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TranslatorSettingsDialog(QWidget *parent, Settings *settings, ModelManager *modelManager);
    ~TranslatorSettingsDialog();

protected:
    void showEvent(QShowEvent *ev);

private slots:
    void on_buttonBox_accepted();
    void on_actionRevealModel_triggered();
    void on_actionDeleteModel_triggered();

private:
    Ui::TranslatorSettingsDialog *ui_;
    Settings *settings_;
    ModelManager *modelManager_;
};

#endif // TRANSLATORSETTINGS_H

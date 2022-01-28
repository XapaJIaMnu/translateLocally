#ifndef TRANSLATORSETTINGS_H
#define TRANSLATORSETTINGS_H

#include <QDialog>
#include <QItemSelection>
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
    void updateSettings();
    void applySettings();
    void revealSelectedModels();
    void deleteSelectedModels();
    void importModels();
    void updateModelActions();

    void on_importRepo_clicked();

private:
    Ui::TranslatorSettingsDialog *ui_;
    Settings *settings_;
    ModelManager *modelManager_;
};

#endif // TRANSLATORSETTINGS_H

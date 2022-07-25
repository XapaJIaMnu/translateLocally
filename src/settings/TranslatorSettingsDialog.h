#ifndef TRANSLATORSETTINGS_H
#define TRANSLATORSETTINGS_H

#include <QDialog>
#include <QItemSelection>
#include <QSortFilterProxyModel>
#include "Settings.h"
#include "inventory/ModelManager.h"
#include "settings/RepositoryTableModel.h"

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
    void updateRepoActions();

    void on_importRepo_clicked();
    void on_deleteRepo_clicked();

    void on_downloadButton_clicked();

    void on_getMoreButton_clicked();

signals:
    void downloadModel(Model model);

private:

    Ui::TranslatorSettingsDialog *ui_;
    Settings *settings_;
    ModelManager *modelManager_;
    QSortFilterProxyModel modelProxy_;
    RepositoryTableModel repositoryModel_;
};

#endif // TRANSLATORSETTINGS_H

#pragma once
#include <QDialog>
namespace Ui {
class AddNewRepoDialog;
}

class AddNewRepoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddNewRepoDialog(QWidget *parent);
    ~AddNewRepoDialog();
    QString getName() const;
    QString getURL() const;

private:
    Ui::AddNewRepoDialog *ui_; // This will be the acces to the widgets defined in .ui
};

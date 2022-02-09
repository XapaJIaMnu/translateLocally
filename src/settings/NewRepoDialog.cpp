#include "NewRepoDialog.h"
#include "ui_NewRepoDialog.h"

AddNewRepoDialog::AddNewRepoDialog(QWidget *parent) :
    QDialog(parent),
    ui_(new Ui::AddNewRepoDialog())
{
    ui_->setupUi(this); // This will init all the widgets
}
AddNewRepoDialog::~AddNewRepoDialog() {
    delete ui_;
}

QString AddNewRepoDialog::getName() const {
    return ui_->nameLine->text();
}

QString AddNewRepoDialog::getURL() const {
    return ui_->URLLine->text();
}


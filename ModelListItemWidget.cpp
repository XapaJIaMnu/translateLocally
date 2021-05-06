#include "ModelListItemWidget.h"
#include "ui_ModelListItemWidget.h"

ModelListItemWidget::ModelListItemWidget(QWidget *parent)
: QWidget(parent)
, ui_(QSharedPointer<Ui::ModelListItemWidget>(new Ui::ModelListItemWidget()))
{
    ui_->setupUi(this);
}

ModelListItemWidget::~ModelListItemWidget()
{
    //
}

void ModelListItemWidget::setModel(LocalModel const &model)
{
    ui_->nameLabel->setText(model.shortName);
    ui_->descriptionLabel->setText(model.name);
    ui_->downloadedLabel->setText(model.type);
}

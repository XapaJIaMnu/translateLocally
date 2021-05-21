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

void ModelListItemWidget::setModel(Model const &model)
{
    ui_->nameLabel->setText(model.shortName);
    ui_->descriptionLabel->setText(model.modelName);
    ui_->downloadedLabel->setText(model.type);
}

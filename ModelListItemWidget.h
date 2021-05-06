#ifndef MODELLISTITEMWIDGET_H
#define MODELLISTITEMWIDGET_H

#include <QWidget>
#include <QSharedPointer>
#include "ModelManager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class ModelListItemWidget; }
QT_END_NAMESPACE

class ModelListItemWidget : public QWidget
{
    Q_OBJECT
public:
    ModelListItemWidget(QWidget *parent = nullptr);
    ~ModelListItemWidget();
    void setModel(LocalModel const &model);
private:
    QSharedPointer<Ui::ModelListItemWidget> ui_;
};

#endif

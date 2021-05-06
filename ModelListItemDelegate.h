#ifndef MODELLISTITEMDELEGATE_H
#define MODELLISTITEMDELEGATE_H

#include <QStyledItemDelegate>
#include "ModelListItemWidget.h"

class ModelListItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    ModelListItemDelegate(QObject *parent = nullptr);
    ~ModelListItemDelegate();
    void setEntry(QModelIndex const &index) const; 
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
private:
    QSharedPointer<ModelListItemWidget> view_;
};

#endif

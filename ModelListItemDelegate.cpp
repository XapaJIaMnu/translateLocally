#include "ModelListItemDelegate.h"
#include <QPainter>

ModelListItemDelegate::ModelListItemDelegate(QObject *parent)
: QStyledItemDelegate(parent)
, view_(QSharedPointer<ModelListItemWidget>(new ModelListItemWidget()))
{
    
}

ModelListItemDelegate::~ModelListItemDelegate()
{
    //
}

void ModelListItemDelegate::setEntry(QModelIndex const &index) const
{
    view_->setModel(qvariant_cast<LocalModel>(index.data(Qt::UserRole)));
}

QSize ModelListItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    setEntry(index);
    view_->adjustSize();

    return view_->sizeHint();
}

void ModelListItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (option.state & QStyle::State_Selected)
    {
        // Paint background
        painter->fillRect(option.rect, option.palette.highlight());

        // Set text color
        painter->setBrush(option.palette.highlightedText());
    }

    setEntry(index);

    view_->resize(option.rect.size());
    painter->save();
    painter->translate(option.rect.topLeft());
    view_->render(painter, QPoint(), QRegion(), QWidget::DrawChildren);
    painter->restore();
}

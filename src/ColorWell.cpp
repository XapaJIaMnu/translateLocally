#include <QColorDialog>
#include <QDebug>
#include "ColorWell.h"

ColorWell::ColorWell(QWidget *parent)
: QToolButton(parent)
, swatch_(16,16)
{
    updateSwatch(color_);
    connect(this, &QToolButton::clicked, this, &ColorWell::openColorDialog);
}

QColor const &ColorWell::color() const
{
    return color_;
}

void ColorWell::setColor(QColor const &color)
{
    color_ = color;
    updateSwatch(color_);
}

void ColorWell::updateColor(QColor const &color)
{
    if (!color.isValid())
        return;
    
    setColor(color);
    emit colorSelected(color_);
}

void ColorWell::openColorDialog()
{
    updateColor(QColorDialog::getColor(color_, this));
}

void ColorWell::updateSwatch(QColor const &color)
{
    swatch_.fill(color);
    icon_ = QIcon(swatch_);
    setIcon(icon_);
}


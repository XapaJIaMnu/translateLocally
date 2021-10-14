#ifndef DACTCOLORWELL_H
#define DACTCOLORWELL_H

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QToolButton>


class ColorWell : public QToolButton
{
    Q_OBJECT
    
public:
    ColorWell(QWidget *parent = 0);
    void setColor(QColor const &color);
    QColor const &color() const;

signals:
    void colorSelected(QColor color);

private slots:
    void openColorDialog();
    void updateColor(QColor const &color);
    void updateSwatch(QColor const &color);

private:
    QColor color_;
    QIcon icon_;
    QPixmap swatch_;
};

#endif

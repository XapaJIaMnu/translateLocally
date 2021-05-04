#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include <QJsonObject>
#include <QTimer>
#include <memory>
#include "Network.h"
#include "ModelManager.h"
#include "ModelListItemDelegate.h"

class MarianInterface;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    // Network temporaries until I figure out a better way
    void onResult(QJsonObject obj);
    void handleDownload(QString filename, QByteArray data);
    void downloadProgress(qint64 ist, qint64 max);

private slots:
    void on_inputBox_textChanged();

    void on_actionTranslate_triggered();

    void on_modelDownload_clicked();

    void on_Models_activated(int index);

    void on_localModels_activated(int index);

    void popupError(QString error);

    void on_FontButton_clicked();

    void translate();

    void translate(QString const &input);

private:
    Ui::MainWindow * ui_; // Sadly QTCreator can't do its job if Ui::MainWindow is wrapped inside a smart ptr, so raw pointer it is
    std::unique_ptr<MarianInterface> translator_;
    void resetTranslator(QString dirname);

    // Keep track of the models
    QStringList urls_;
    QStringList codes_;
    QStringList names_;

    // Model and config manager
    ModelManager models_;
    ModelListItemDelegate localModelDelegate_;

    // Network code:
    Network network_;

    QTimer inactivityTimer_;
    QString translationInput_;
};
#endif // MAINWINDOW_H

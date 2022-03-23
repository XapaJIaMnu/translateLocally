#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include <QJsonObject>
#include <QPointer>
#include "AlignmentHighlighter.h"
#include "AlignmentWorker.h"
#include "Network.h"
#include "inventory/ModelManager.h"
#include "settings/TranslatorSettingsDialog.h"
#include "settings/Settings.h"

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
    void handleDownload(QFile *file, QString filename);
    void downloadProgress(qint64 ist, qint64 max);
    void updateModelSettings(size_t memory, size_t cores);

    enum Action {
        FetchRemoteModels
    };
    Q_ENUM(Action);

signals:
    // Emitted at the end of MainWindow's initialisation
    void launched();

public slots:
    

private slots:
    void on_inputBox_textChanged();

    void on_inputBox_cursorPositionChanged();

    void on_outputBox_cursorPositionChanged();

    void on_translateAction_triggered();

    void on_translateButton_clicked();

    void on_fontAction_triggered();

    void on_actionTranslator_Settings_triggered();

    void on_localModels_activated(int index);

    void on_actionSplit_Horizontally_triggered();

    void on_actionSplit_Vertically_triggered();

    void popupError(QString error);

    void popupWarning(QString warning);

    void translate();

    void translate(QString const &input);

    void updateLocalModels();

    void updateSelectedModel();

    bool registerNativeMessagingAppManifest();

protected: // We use this for first run dialog
    void showEvent(QShowEvent *ev);
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    Q_INVOKABLE void showFirstRunHelper();
#else
    void showFirstRunHelper();
#endif

private:
    Ui::MainWindow * ui_; // Sadly QTCreator can't do its job if Ui::MainWindow is wrapped inside a smart ptr, so raw pointer it is

    // Note: these QPointers are freed through parent/child relation
    QPointer<AlignmentWorker> alignmentWorker_;
    QPointer<AlignmentHighlighter> highlighter_;

    // Translator related settings
    QPointer<MarianInterface> translator_;
    Translation translation_;

    void resetTranslator();
    void showDownloadPane(bool visible);
    void downloadModel(Model model);

    // Keep track of the models
    QStringList urls_;
    QStringList codes_;
    QStringList names_;

    // Model and config manager
    Settings settings_;
    ModelManager models_;
    TranslatorSettingsDialog translatorSettingsDialog_;

    // Network code:
    Network network_;

    // Little utility to connect settings to callbacks that initialise and 
    // update them. Part of class def because connect() is a QObject method.
    template <typename T, typename Fun>
    void bind(SettingImpl<T> &setting, Fun update) {
        // Update initially
        update(setting.value());

        // Update every time it changes
        connect(&setting, &Setting::valueChanged, [&, update]{
            update(setting.value());
        });
    }
};
#endif // MAINWINDOW_H

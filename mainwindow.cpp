#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <memory>
#include <functional>
#include "MarianInterface.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QSaveFile>
#include <QDir>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , models_(ModelManager(this))
    , network_(Network(this))
    , translator_(nullptr)
{
    ui->setupUi(this);

    // Initial text of the comboBox
    ui->Models->insertItem(0, QString("Models"));

    // Hide download progress bar
    ui->progressBar->hide();

    // Display local models
    for (auto&& item : models_.models_) {
        ui->localModels->addItem(item.name);
    }
    // Load one if we have
    if (models_.models_.size() != 0) {
        resetTranslator(models_.models_[0].path);
    }
    // @TODO something is broken, this gets called n+1 times with every new model
    // This updates the local models and activates the newly downloaded one.
    auto updateLocalModels = [&](int index){
        ui->localModels->addItem(models_.models_[index].name);
        on_localModels_activated(index);
        ui->localModels->setCurrentIndex(index);
    };
    connect(&models_, &ModelManager::newModelAdded, this, updateLocalModels);
}

MainWindow::~MainWindow()
{
    delete ui;
    if (translator_) {
        delete  translator_; // Free the translator
    }
}


void MainWindow::on_translateButton_clicked()
{
    ui->localModels->setEnabled(false); // Disable changing the model while translating
    ui->translateButton->setEnabled(false); //Disable the translate button before the translation finishes
    if (translator_) {
        if (ui->inputBox->toPlainText() != QString("")) {
            ui->outputBox->setText("Translating, please wait...");
            this->repaint(); // Force update the UI before the translation starts so that it can show the previous text
            QString translation = translator_->translate(ui->inputBox->toPlainText());
            ui->outputBox->setText(translation);
        } else {
            // Empty input crashes the translator
            ui->outputBox->setText("Write something to be translated first.");
        }
    } else {
        ui->outputBox->setText("You need to download a translation model first. Do that with the interface on the right.");
    }
    ui->localModels->setEnabled(true); // Re-enable model changing
    ui->translateButton->setEnabled(true); // Re-enable button after translation is done
}

/**
 * @brief MainWindow::on_modelDownload_clicked fetches the available models json from the Internet.
 */
void MainWindow::on_modelDownload_clicked()
{
    QString url("http://data.statmt.org/bergamot/models/models.json");
    connect(&network_, &Network::getJson, this, &MainWindow::onResult);
    network_.downloadJson(url);
}

/**
 * @brief MainWindow::onResult reads the json for available models
 */
void MainWindow::onResult(QJsonObject obj, QString err)
{
    static bool success = false;
    if (err != "") {
        if (!success) {
            ui->Models->removeItem(0);
            ui->Models->insertItem(0, QString("No internet connection"));
            ui->outputBox->setText(err);
        }
    } else if (!success) { // Success
        QJsonArray array = obj["models"].toArray();
        for (auto&& arrobj : array) {
            QString name = arrobj.toObject()["name"].toString();
            QString code = arrobj.toObject()["code"].toString();
            QString url = arrobj.toObject()["url"].toString();
            urls_.append(url);
            codes_.append(code);
            names_.append(name);
        }
        ui->Models->removeItem(0);
        ui->Models->insertItems(0, codes_);
        success = true;
    }
}

void MainWindow::handleDownload(QString filename, QByteArray data , QString err) {
    if (err == QString("")) {
        QString myerr = models_.writeModel(filename, data);
        if (myerr != QString("")) {
            ui->outputBox->append(myerr);
        }
    } else {
        ui->outputBox->append(err);
    }
    // Re-enable model downloading interface:
    ui->Models->setEnabled(true);
}

/**
 * @brief MainWindow::on_Models_activated Download available models or setup new ones
 * @param index
 */
void MainWindow::on_Models_activated(int index)
{
    if (names_.size() == 0) { // Fetch the models if they are not there
        on_modelDownload_clicked();
    } else { // If models are fetched download the selected model
        //@TODO check if model is already downloaded and prompt user for download confirmation
        connect(&network_, &Network::progressBar, this, &MainWindow::downloadProgress);
        connect(&network_, &Network::downloadComplete, this, &MainWindow::handleDownload);
        network_.downloadFile(urls_[index]);
        // Disable this section of the ui while a model is downloading..
        ui->Models->setEnabled(false);
    }
}

void MainWindow::downloadProgress(qint64 ist, qint64 max) {
    ui->progressBar->show();
    ui->progressBar->setRange(0,max);
    ui->progressBar->setValue(ist);
    if(max < 0) {
        ui->progressBar->hide();
    }
}

/**
 * @brief MainWindow::on_localModels_activated Change the loaded translation model to something else.
 * @param index index of model in models_.models_ (model manager)
 */

void MainWindow::on_localModels_activated(int index) {
    if (models_.models_.size() > 0) {
        resetTranslator(models_.models_[index].path);
    }
}

void MainWindow::resetTranslator(QString dirname) {
    QString model0_path = dirname + "/";
    ui->localModels->setEnabled(false); // Disable changing the model while changing the model
    ui->translateButton->setEnabled(false); //Disable the translate button before the swap

    if (translator_) {
        delete translator_; // We need to first call the destructor otherwise we run into a crash in spdlog. No smart pointers reset.
    }
    translator_ = new MarianInterface(model0_path);

    ui->translateButton->setEnabled(true); // Reenable it
    ui->localModels->setEnabled(true); // Disable changing the model while changing the model
}

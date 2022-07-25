#include "TranslatorSettingsDialog.h"
#include "FilterTableView.h"
#include "Translation.h"
#include "settings/RepositoryTableModel.h"
#include "ui_TranslatorSettingsDialog.h"
#include "NewRepoDialog.h"
#include <thread>
#include <QDesktopServices>
#include <QUrl>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>


TranslatorSettingsDialog::TranslatorSettingsDialog(QWidget *parent, Settings *settings, ModelManager *modelManager)
: QDialog(parent)
, ui_(new Ui::TranslatorSettingsDialog())
, settings_(settings)
, modelManager_(modelManager)
, modelProxy_(this)
, repositoryModel_(this)
{
    ui_->setupUi(this);

    // Set up sorting & filtering proxy for the model table model
    modelProxy_.setSourceModel(modelManager_);
    modelProxy_.setFilterKeyColumn(-1); // filter rows based on all columns
    modelProxy_.setFilterCaseSensitivity(Qt::CaseInsensitive);

    // Create lists of memory and cores
    QList<unsigned int> memory_options = {64, 128, 256, 512, 768, 1024, 1280, 1536, 1762, 2048};

    QList<unsigned int> cores_options;
    size_t max_cores = std::thread::hardware_concurrency();
    
    for (size_t cores = max_cores; cores > 0; cores -= 2)
        cores_options.prepend(cores);
    
    cores_options.prepend(1);

    for (auto option : memory_options)
        ui_->memoryBox->addItem(QString("%1").arg(option), option);

    for (auto option : cores_options)
        ui_->coresBox->addItem(QString("%1").arg(option), option);

    ui_->localModelTable->setModel(&modelProxy_);
    ui_->localModelTable->setSortingEnabled(true);
    ui_->localModelTable->horizontalHeader()->setSectionResizeMode(ModelManager::Column::Source, QHeaderView::ResizeToContents);
    ui_->localModelTable->horizontalHeader()->setSectionResizeMode(ModelManager::Column::Target, QHeaderView::ResizeToContents);
    ui_->localModelTable->horizontalHeader()->setSectionResizeMode(ModelManager::Column::Type, QHeaderView::Stretch);
    ui_->localModelTable->horizontalHeader()->setSectionResizeMode(ModelManager::Column::Repo, QHeaderView::ResizeToContents);
    ui_->localModelTable->horizontalHeader()->setSectionResizeMode(ModelManager::Column::LocalVer, QHeaderView::ResizeToContents);
    ui_->localModelTable->horizontalHeader()->setSectionResizeMode(ModelManager::Column::RemoteVer, QHeaderView::ResizeToContents);
    
    // Changing filtering text in the table model updates the filter used by the model proxy. This also
    // sets filterWildcard to '' when the filter text field is hidden.
    connect(ui_->localModelTable, &FilterTableView::filterTextChanged, &modelProxy_, &QSortFilterProxyModel::setFilterWildcard);

    ui_->repoTable->setModel(&repositoryModel_);
    ui_->repoTable->horizontalHeader()->setSectionResizeMode(RepositoryTableModel::Column::Name, QHeaderView::ResizeToContents);
    ui_->repoTable->horizontalHeader()->setSectionResizeMode(RepositoryTableModel::Column::URL, QHeaderView::Stretch);
    connect(&repositoryModel_, &RepositoryTableModel::warning, this, [&](QString warning) {
        QMessageBox::warning(this, tr("Warning"), warning);
    });

    connect(ui_->localModelTable->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TranslatorSettingsDialog::updateModelActions);
    connect(ui_->actionRevealModel, &QAction::triggered, this, &TranslatorSettingsDialog::revealSelectedModels);
    connect(ui_->actionDeleteModel, &QAction::triggered, this, &TranslatorSettingsDialog::deleteSelectedModels);
    connect(ui_->deleteModelButton, &QPushButton::clicked, this, &TranslatorSettingsDialog::deleteSelectedModels);
    connect(ui_->importModelButton, &QPushButton::clicked, this, &TranslatorSettingsDialog::importModels);
    // Repository actions
    connect(ui_->actionDeleteRepo, &QAction::triggered, this, &TranslatorSettingsDialog::on_deleteRepo_clicked);
    connect(ui_->repoTable->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TranslatorSettingsDialog::updateRepoActions);

    // Hide RemoteVer column if we don't have those available
    ui_->localModelTable->setColumnHidden(ModelManager::Column::RemoteVer, modelManager_->getRemoteModels().isEmpty());

    // Connections with the modelManager: Re-enable the button after fetching the remote models
    connect(modelManager_, &ModelManager::fetchedRemoteModels, this, [&](){
        ui_->localModelTable->showColumn(ModelManager::Column::RemoteVer);
        ui_->getMoreButton->setEnabled(true);
    });

    connect(this, &QDialog::accepted, this, &TranslatorSettingsDialog::applySettings);

    // Update context menu state on start-up
    updateModelActions();
    updateRepoActions();
}

TranslatorSettingsDialog::~TranslatorSettingsDialog()
{
    delete ui_;
}

void TranslatorSettingsDialog::updateSettings()
{
    ui_->coresBox->setCurrentIndex(ui_->coresBox->findData(settings_->cores()));
    ui_->memoryBox->setCurrentIndex(ui_->memoryBox->findData(settings_->workspace()));
    ui_->translateImmediatelyCheckbox->setChecked(settings_->translateImmediately());
    ui_->showAligmentsCheckbox->setChecked(settings_->showAlignment());
    ui_->alignmentColorButton->setColor(settings_->alignmentColor());
    ui_->syncScrollingCheckbox->setChecked(settings_->syncScrolling());
    ui_->cacheTranslationsCheckbox->setChecked(settings_->cacheTranslations());
    repositoryModel_.load(settings_->externalRepos.value());
}

void TranslatorSettingsDialog::applySettings()
{
    settings_->cores.setValue(ui_->coresBox->currentData().toUInt());
    settings_->workspace.setValue(ui_->memoryBox->currentData().toUInt());
    settings_->translateImmediately.setValue(ui_->translateImmediatelyCheckbox->isChecked());
    settings_->showAlignment.setValue(ui_->showAligmentsCheckbox->isChecked());
    settings_->alignmentColor.setValue(ui_->alignmentColorButton->color());
    settings_->syncScrolling.setValue(ui_->syncScrollingCheckbox->isChecked());
    settings_->cacheTranslations.setValue(ui_->cacheTranslationsCheckbox->isChecked());
    settings_->externalRepos.setValue(repositoryModel_.dump());
}

void TranslatorSettingsDialog::revealSelectedModels()
{
    for (auto&& index : ui_->localModelTable->selectionModel()->selectedIndexes()) {
        Model model = modelManager_->data(index, Qt::UserRole).value<Model>();
        QDesktopServices::openUrl(QUrl(QString("file://%1").arg(model.path), QUrl::TolerantMode));
    }
}

void TranslatorSettingsDialog::deleteSelectedModels()
{
    QVector<Model> selection;
    for (auto&& index : ui_->localModelTable->selectionModel()->selectedRows(0)) {
        Model model = modelManager_->data(index, Qt::UserRole).value<Model>();
        if (modelManager_->isManagedModel(model))
            selection.append(model);
    }

    if (QMessageBox::question(this,
            tr("Delete models"),
            tr("Are you sure you want to delete %n language model(s)?", "", selection.size())
        ) != QMessageBox::Yes)
        return;

    for (auto &&model : selection)
        modelManager_->removeModel(model);
}

void TranslatorSettingsDialog::importModels() {
    QStringList paths = QFileDialog::getOpenFileNames(this,
        tr("Open Translation model"),
        QString(),
        tr("Packaged translation model (*.tar.gz)"));

    for (QString const &path : paths) {
        QFile file(path);
        modelManager_->writeModel(&file);
    }
}

void TranslatorSettingsDialog::showEvent(QShowEvent *ev)
{
    // When this dialog pops up, update all widgets to reflect the current
    // settings.
    updateSettings();
}

void TranslatorSettingsDialog::updateModelActions()
{
    bool containsLocalModel = false;
    bool containsDeletableModel = false;
    bool containsOutdatedModel = false;
    bool containsDownloadableModel = false;

    for (auto&& index : ui_->localModelTable->selectionModel()->selectedIndexes()) {
        Model model = modelProxy_.data(index, Qt::UserRole).value<Model>();
        
        if (model.isLocal())
            containsLocalModel = true;

        if (modelManager_->isManagedModel(model))
            containsDeletableModel = true;

        if (model.outdated())
            containsOutdatedModel = true;

        if (model.isRemote())
            containsDownloadableModel = true;
    }

    ui_->actionRevealModel->setEnabled(containsLocalModel);
    ui_->actionDeleteModel->setEnabled(containsDeletableModel);
    ui_->deleteModelButton->setEnabled(containsDeletableModel);
    ui_->downloadButton->setEnabled(containsDownloadableModel || containsOutdatedModel);
}

void TranslatorSettingsDialog::updateRepoActions()
{
    bool containsDeletableRepo = false;

    for (auto&& index : ui_->repoTable->selectionModel()->selectedIndexes()) {
        if (repositoryModel_.canRemove(index)) {
            containsDeletableRepo = true;
            break;
        }
    }

    ui_->actionDeleteRepo->setEnabled(containsDeletableRepo);
    ui_->deleteRepo->setEnabled(containsDeletableRepo);
}

void TranslatorSettingsDialog::on_importRepo_clicked()
{
    AddNewRepoDialog prompt(this);
    if (prompt.exec() == QDialog::Accepted) {
        if (!prompt.getURL().isEmpty()) {
            repositoryModel_.insert(prompt.getName(), prompt.getURL());
        }
    }
}


void TranslatorSettingsDialog::on_deleteRepo_clicked()
{
    auto selection = ui_->repoTable->selectionModel()->selectedRows(0);

    if (QMessageBox::question(this,
            tr("Remove repo"),
            tr("Are you sure you want to remove %n selected repo(s)?", "", selection.size())
        ) != QMessageBox::Yes)
        return;

    repositoryModel_.removeRows(selection);
}


void TranslatorSettingsDialog::on_downloadButton_clicked()
{
    if (ui_->localModelTable->selectionModel()->selectedRows().size() != 1) {
        // Can only download one model at a time for now
        QMessageBox::warning(this, tr("Warning"), "Can only download one model at a time for now. Please select just one model.");
    } else {
        Model model = modelProxy_.data(ui_->localModelTable->selectionModel()->selectedIndexes().first(), Qt::UserRole).value<Model>();
        
        // If this is a local model, we want to update instead?
        if (model.isLocal()) {
            auto update = modelManager_->findModelForUpdate(model);
            if (!update) {
                QMessageBox::warning(this, tr("Error"), "Seems like we can't find a model to update. Submit a bug please :(.");
                return;
            }
            model = update.value();
        }

        emit downloadModel(model);
        this->setVisible(false); // Hide the settings window so we can see the download.
    }
}


void TranslatorSettingsDialog::on_getMoreButton_clicked()
{
    modelManager_->fetchRemoteModels();
    ui_->getMoreButton->setEnabled(false);
}

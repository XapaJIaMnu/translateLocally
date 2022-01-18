#include "MarianInterface.h"
#include "3rd_party/bergamot-translator/src/translator/service.h"
#include "3rd_party/bergamot-translator/src/translator/service.h"
#include "3rd_party/bergamot-translator/src/translator/parser.h"
#include "3rd_party/bergamot-translator/src/translator/response.h"
#include "3rd_party/bergamot-translator/3rd_party/marian-dev/src/3rd_party/spdlog/spdlog.h"
#include <future>
#include <memory>
#include <thread>
#include <chrono>
#include <QMutexLocker>

namespace  {

std::shared_ptr<marian::Options> makeOptions(const std::string &path_to_model_dir, const translateLocally::marianSettings &settings) {
    std::shared_ptr<marian::Options> options(marian::bergamot::parseOptionsFromFilePath(path_to_model_dir + "/config.intgemm8bitalpha.yml"));
    options->set("cpu-threads", settings.cpu_threads,
                 "workspace", settings.workspace,
                 "mini-batch-words", 1000,
                 "alignment", "soft",
                 "quiet", true);
    return options;
}

int countWords(std::string input) {
    const char * str = input.c_str();

    bool inSpaces = true;
    int numWords = 0;

    while (*str != '\0') {
        if (std::isspace(*str)) {
            inSpaces = true;
        } else if (inSpaces) {
            numWords++;
            inSpaces = false;
        }
        ++str;
    }
    return numWords;
}

} // Anonymous namespace

struct ModelDescription {
    std::string config_file;
    translateLocally::marianSettings settings;
};

MarianInterface::MarianInterface(QObject *parent)
    : QObject(parent)
    , pendingInput_(nullptr)
    , pendingModel_(nullptr) {

    // This worker is the only thread that can interact with Marian. Right now
    // it basically uses marian::bergamot::Service's non-blocking interface
    // in a blocking way because std::future is not compatible with Qt's event
    // loop, and QtConcurrent::run would not work as calls to
    // Service::translate() are not thread-safe.
    // This worker basically processes a command queue, except that there are
    // only two possible commands: load model & translate input. And there are
    // no actual queues because we always want the last command: we don't care
    // about previously pending models or translations. The semaphore
    // indicates whether there are 0, 1, or 2 commands pending. If a command
    // is pending but both "queues" are empty, we'll treat that as a shutdown
    // request.
    worker_ = std::thread([&]() {
        std::unique_ptr<marian::bergamot::AsyncService> service;
        std::shared_ptr<marian::bergamot::TranslationModel> model;

        while (true) {
            std::unique_ptr<ModelDescription> modelChange;
            std::unique_ptr<std::string> input;

            // Wait for work
            commandIssued_.acquire();

            {
                // Lock while working with the pointers
                QMutexLocker locker(&lock_);

                // First check whether the command is loading a new model
                if (pendingModel_)
                    modelChange = std::move(pendingModel_);
                
                // Second check whether command is translating something.
                // Note: else if because we only process one command per
                // iteration otherwise commandIssued_ would go out of sync.
                else if (pendingInput_)
                    input = std::move(pendingInput_);
                
                // Command without any pending change -> poison.
                else
                    break;
            }
            
            emit pendingChanged(true);

            try {
                if (modelChange) {
                    // Reconstruct the service because cpu_threads might have changed.
                    // @TODO: don't recreate Service if cpu_threads didn't change?
                    marian::bergamot::AsyncService::Config serviceConfig;
                    serviceConfig.numWorkers = modelChange->settings.cpu_threads;
                    serviceConfig.cacheEnabled = modelChange->settings.translation_cache;
                    serviceConfig.cacheSize = kTranslationCacheSize;
                    serviceConfig.cacheMutexBuckets = modelChange->settings.cpu_threads;
                    
                    // Free up old service first (see https://github.com/browsermt/bergamot-translator/issues/290)
                    service.reset();

                    service = std::make_unique<marian::bergamot::AsyncService>(serviceConfig);

                    // Initialise a new model. Old model will be released if
                    // service is done with it, which it is since all translation
                    // requests are effectively blocking in this thread.
                    auto modelConfig = makeOptions(modelChange->config_file, modelChange->settings);
                    model = std::make_shared<marian::bergamot::TranslationModel>(modelConfig, marian::bergamot::MemoryBundle{}, modelChange->settings.cpu_threads);
                } else if (input) {
                    if (model) {
                        std::future<int> wordCount = std::async(countWords, *input); // @TODO we're doing an "unnecessary" string copy here (necessary because we std::move input into service->translate)

                        marian::bergamot::ResponseOptions options;
                        options.alignment = true;
                        
                        // Using promise for a translation, and future for waiting
                        // for that translation to turn the async service into
                        // a blocking request.
                        std::promise<marian::bergamot::Response> response;
                        auto future = response.get_future();
                        
                        // Measure the time it takes to queue and respond to the
                        // translation request
                        auto start = std::chrono::steady_clock::now(); // Time the translation
                        service->translate(model, std::move(*input), [&](auto &&val) { response.set_value(std::move(val)); }, options);
                        future.wait();
                        auto end = std::chrono::steady_clock::now();
                        
                        // Calculate translation speed in terms of words per second
                        double words = wordCount.get();
                        std::chrono::duration<double> elapsedSeconds = end-start;
                        int translationSpeed = std::ceil(words/elapsedSeconds.count()); // @TODO this could probably be done in the service in the future
                        emit translationReady(Translation(std::move(future.get()), translationSpeed));
                    } else {
                        // TODO: What? Raise error? Set model_ to ""?
                    }
                }
            } catch (const std::runtime_error &e) {
                emit error(QString::fromStdString(e.what()));
            }

            emit pendingChanged(false);
        }
    });
}

QString const &MarianInterface::model() const {
    return model_;
}

void MarianInterface::setModel(QString path_to_model_dir, const translateLocally::marianSettings &settings) {
    model_ = path_to_model_dir;

    // Empty model string means just "unload" the model. We don't do that (yet),
    // instead this just causes translate(QString) to no longer work.
    if (model_.isEmpty())
        return;

    // move my shared_ptr from stack to heap
    QMutexLocker locker(&lock_);
    std::unique_ptr<ModelDescription> model(new ModelDescription{model_.toStdString(), settings});
    std::swap(pendingModel_, model);

    // notify worker if there wasn't already a pending model
    if (!model)
        commandIssued_.release();
}

void MarianInterface::translate(QString in) {
    // If we don't have a model yet (loaded, or queued to be loaded, doesn't matter)
    // then don't bother trying to translate something.
    if (model_.isEmpty())
        return;

    QMutexLocker locker(&lock_);
    std::unique_ptr<std::string> input(new std::string(in.toStdString()));
    std::swap(pendingInput_, input);
    
    if (!input)
        commandIssued_.release();
}

MarianInterface::~MarianInterface() {
    // Remove all pending changes and unlock worker (which will then break.)
    {
        QMutexLocker locker(&lock_);
        auto model = std::move(pendingModel_);
        auto input = std::move(pendingInput_);

        if (!input && !model)
            commandIssued_.release();
    }
    
    // Wait for worker to join as it depends on resources we still own.
    worker_.join();
}


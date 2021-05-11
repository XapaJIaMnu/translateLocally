#include "MarianInterface.h"
#include "3rd_party/bergamot-translator/src/translator/service.h"
#include "3rd_party/bergamot-translator/src/translator/service.h"
#include "3rd_party/bergamot-translator/src/translator/parser.h"
#include "3rd_party/bergamot-translator/src/translator/response.h"
#include "3rd_party/bergamot-translator/3rd_party/marian-dev/src/3rd_party/spdlog/spdlog.h"
#include <memory>
#include <thread>
#include <QMutexLocker>


namespace  {
marian::Ptr<marian::Options> MakeOptions(const std::string &path_to_model_dir, translateLocally::marianSettings& settings) {
    std::string model_path = path_to_model_dir + "config.intgemm8bitalpha.yml";
    std::vector<std::string> args = {"marian-decoder", "-c", model_path,
                                     "--cpu-threads", std::to_string(settings.getCores()),
                                     "--workspace", std::to_string(settings.getWorkspace()),
                                     "--mini-batch-words", "1000"};

    std::vector<char *> argv;
    argv.reserve(args.size());

    for (size_t i = 0; i < args.size(); ++i) {
        argv.push_back(const_cast<char *>(args[i].c_str()));
    }
    auto cp = marian::bergamot::createConfigParser();
    auto options = cp.parseOptions(argv.size(), &argv[0], true);
    return options;
}

} // Anonymous namespace

struct ModelDescription {
    std::string config;
    translateLocally::marianSettings settings;
};

MarianInterface::MarianInterface(QObject *parent)
    : QObject(parent)
    , pendingInput_(nullptr)
    , pendingModel_(nullptr) {
    worker_ = std::thread([&]() {
        std::unique_ptr<marian::bergamot::Service> service;

        while (true) {
            std::unique_ptr<ModelDescription> model;
            std::unique_ptr<std::string> input;

            // Wait for work
            commandIssued_.acquire();

            {
                // Lock while working with the pointers
                QMutexLocker locker(&lock_);

                // First check whether the command is loading a new model
                if (pendingModel_)
                    model = std::move(pendingModel_);
                
                // Second check whether command is translating something.
                else if (pendingInput_)
                    input = std::move(pendingInput_);
                
                // Command without any pending change -> poison.
                else
                    break;
            }
            
            emit pendingChanged(true);
            
            if (model) {
                // Unload marian first (so we can delete loggers after that)
                service.reset();

                // We need to manually destroy the loggers, as marian doesn't do
                // that but will complain when a new marian::Config tries to 
                // initialise loggers with the same name.
                spdlog::drop("general");
                spdlog::drop("valid");

                service.reset(new marian::bergamot::Service(MakeOptions(model->config, model->settings)));
            } else if (input) {
                std::future<marian::bergamot::Response> responseFuture = service->translate(std::move(*input));
                responseFuture.wait();
                marian::bergamot::Response response = responseFuture.get();
                emit translationReady(QString::fromStdString(response.target.text));
            }

            emit pendingChanged(false);
        }
    });
}

QString const &MarianInterface::model() const {
    return model_;
}

void MarianInterface::setModel(QString path_to_model_dir, translateLocally::marianSettings& settings) {
    model_ = path_to_model_dir;
    
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


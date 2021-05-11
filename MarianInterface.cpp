#include "MarianInterface.h"
#include "3rd_party/bergamot-translator/src/translator/service.h"
#include "3rd_party/bergamot-translator/src/translator/service.h"
#include "3rd_party/bergamot-translator/src/translator/parser.h"
#include "3rd_party/bergamot-translator/src/translator/response.h"
#include "3rd_party/bergamot-translator/3rd_party/marian-dev/src/3rd_party/spdlog/spdlog.h"
#include <memory>
#include <thread>


namespace  {
marian::Ptr<marian::Options> MakeOptions(QString path_to_model_dir, translateLocally::marianSettings& settings) {
    std::string model_path = path_to_model_dir.toStdString() + "config.intgemm8bitalpha.yml";
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

MarianInterface::MarianInterface(QString path_to_model_dir, translateLocally::marianSettings& settings, QObject *parent)
    : QObject(parent)
    , pendingInput_()
    , pendingInputCount_(0)
    , mymodel(path_to_model_dir) {

    worker_ = std::thread([&](marian::Ptr<marian::Options> options) {
        marian::bergamot::Service service(options);

        while (true) {
            // Wait for work
            pendingInputCount_.acquire();

            std::unique_ptr<std::string> input(pendingInput_.fetchAndStoreAcquire(nullptr));
            // Empty ptr? pendingInputCount released without valid pointer -> poison (or a bug)
            if (!input)
                break;

            emit pendingChanged(true);

            std::future<marian::bergamot::Response> responseFuture = service.translate(std::move(*input));
            responseFuture.wait();
            marian::bergamot::Response response = responseFuture.get();
            emit translationReady(QString::fromStdString(response.target.text));

            emit pendingChanged(false);
        }

        // We need to manually destroy the loggers, as marian doesn't do that.
        spdlog::drop("general");
        spdlog::drop("valid");
    }, MakeOptions(path_to_model_dir, settings));
}

void MarianInterface::translate(QString in) {
    std::unique_ptr<std::string> old(pendingInput_.fetchAndStoreAcquire(new std::string(in.toStdString())));

    // notify worker (but only if there wasn't already a pending task)
    if (!old)
        pendingInputCount_.release();
}

MarianInterface::~MarianInterface() {
    // Poision the worker with a nullptr
    std::unique_ptr<std::string> old(pendingInput_.fetchAndStoreAcquire(nullptr));
    if (!old)
        pendingInputCount_.release();
    
    // Wait for worker to join as it depends on resources we still own.
    // TODO: This might take a long time, can we just .detach() it instead somehow?
    worker_.join();
}

bool MarianInterface::pending() const {
    return pendingInputCount_.available() != 0;
}

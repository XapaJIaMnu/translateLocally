#include "MarianInterface.h"
#include "3rd_party/bergamot-translator/src/translator/service.h"
#include "3rd_party/bergamot-translator/src/translator/service.h"
#include "3rd_party/bergamot-translator/src/translator/parser.h"
#include "3rd_party/bergamot-translator/src/translator/response.h"
#include "3rd_party/bergamot-translator/3rd_party/marian-dev/src/3rd_party/spdlog/spdlog.h"
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
    , service_(new marian::bergamot::Service(MakeOptions(path_to_model_dir, settings)))
    , serial_(0)
    , finished_(0)
    , mymodel(path_to_model_dir) {}

void MarianInterface::translate(QString in) {
    // Wait on future until Response is complete. Since the future doesn't have a callback or anything
    // we should put all the processing in a background thread. Normally, if we have a future, we expect
    // that future to have a method that allows to attach a callback, but this is reserved for c++20? c++22
    // We have to copy any member variables we use (I'm looking at you QString input, because QString is copy-on-write)
    auto translateAndSignal = [&](std::string &&input, std::size_t serial) {
        using marian::bergamot::Response;
        std::future<marian::bergamot::Response> responseFuture = service_->translate(std::move(input));
        responseFuture.wait();
        marian::bergamot::Response response = responseFuture.get();

        // There is no guarantee that we get/process responses in the same order
        // as we sent sentences to be translated. So let's make sure we haven't
        // been overtaken by a further progressed sentence before emitting the
        // translation.
        // TODO: race condition on finished_?
        if (serial < finished_)
            return;
        
        finished_ = serial;
        emit translationReady(QString::fromStdString(response.target.text));
    };

    std::thread mythread(translateAndSignal, in.toStdString(), ++serial_);
    mythread.detach();
}

MarianInterface::~MarianInterface() {
    // We need to manually destroy the loggers, as marian doesn't do that.
    spdlog::drop("general");
    spdlog::drop("valid");
}


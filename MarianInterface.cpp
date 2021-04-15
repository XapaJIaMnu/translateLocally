#include "marianinterface.h"
#include "3rd_party/bergamot-translator/src/translator/service.h"
#include "3rd_party/bergamot-translator/src/translator/parser.h"
#include "3rd_party/bergamot-translator/src/translator/response.h"

namespace  {
marian::Ptr<marian::Options> MakeOptions() {
    std::vector<std::string> args = {"marian-decoder", "-c", "/mnt/Storage/students/esen/enes.student.tiny11/config.yml", "--cpu-threads", "8", "--mini-batch-words", "1000"};

    std::vector<char *> argv;
    argv.reserve(args.size());

    for (int i = 0; i < args.size(); ++i) {
        argv.push_back(const_cast<char *>(args[i].c_str()));
    }
    auto cp = marian::bergamot::createConfigParser();
    auto options = cp.parseOptions(argv.size(), &argv[0], true);
    return options;
}
} // Anonymous namespace

MarianInterface::MarianInterface() : service_(MakeOptions()) {

}

QString MarianInterface::translate(QString in) {
    std::string input = in.toStdString();
    using marian::bergamot::Response;

    // Wait on future until Response is complete
    std::future<marian::bergamot::Response> responseFuture = service_.translate(std::move(input));
    responseFuture.wait();
    marian::bergamot::Response response = responseFuture.get();

    return QString::fromStdString(response.target.text);
}


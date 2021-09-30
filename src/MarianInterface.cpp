#include "MarianInterface.h"
#include "3rd_party/bergamot-translator/src/translator/service.h"
#include "3rd_party/bergamot-translator/src/translator/service.h"
#include "3rd_party/bergamot-translator/src/translator/parser.h"
#include "3rd_party/bergamot-translator/src/translator/response.h"
#include "3rd_party/bergamot-translator/3rd_party/marian-dev/src/3rd_party/spdlog/spdlog.h"
#include <memory>
#include <thread>
#include <chrono>
#include <QMutexLocker>
#include <QDebug>

namespace  {
marian::Ptr<marian::Options> MakeOptions(const std::string &path_to_model_dir, translateLocally::marianSettings& settings) {
    std::string model_path = path_to_model_dir + "/config.intgemm8bitalpha.yml";
    std::vector<std::string> args = {"marian-decoder", "-c", model_path,
                                     "--cpu-threads", std::to_string(settings.cpu_threads),
                                     "--workspace", std::to_string(settings.workspace),
                                     "--mini-batch-words", "1000",
                                     "--alignment", "0.2",
                                     "--quiet"};

    std::vector<char *> argv;
    argv.reserve(args.size());

    for (size_t i = 0; i < args.size(); ++i) {
        argv.push_back(const_cast<char *>(args[i].c_str()));
    }
    auto cp = marian::bergamot::createConfigParser();
    auto options = cp.parseOptions(argv.size(), &argv[0], true);
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

bool contains(marian::bergamot::ByteRange const &span, std::size_t offset) {
    // return offset >= span.begin && offset < span.end;

    // Note: technically incorrect, but gives better user experience for those
    // not aware exactly what is happening with sentence piece tokens.
    return offset > span.begin && offset <= span.end;
}

bool findWordByByteOffset(marian::bergamot::Annotation const &annotation, std::size_t pos, std::size_t &sentenceIdx, std::size_t &wordIdx) {
    for (sentenceIdx = 0; sentenceIdx < annotation.numSentences(); ++sentenceIdx) {
        if (::contains(annotation.sentence(sentenceIdx), pos))
            break;
    }

    if (sentenceIdx == annotation.numSentences())
        return false;
    
    for (wordIdx = 0; wordIdx < annotation.numWords(sentenceIdx); ++wordIdx)
        if (::contains(annotation.word(sentenceIdx, wordIdx), pos))
            break;

    return wordIdx != annotation.numWords(sentenceIdx);
}

qsizetype offsetToPosition(std::string const &text, std::size_t offset) {
    if (offset > text.size()) {
        qDebug() << "Warning:" << offset << ">" << text.size();
        return -1;
    }

    return QString::fromUtf8(text.data(), offset).size();
}

std::size_t positionToOffset(std::string const &text, qsizetype pos) {
    return QString::fromStdString(text).left(pos).toLocal8Bit().size();
}

} // Anonymous namespace



Translation::Translation()
: response_()
, speed_(-1) {
    //
}

Translation::Translation(marian::bergamot::Response &&response, int speed)
: response_(std::make_shared<marian::bergamot::Response>(std::move(response)))
, speed_(speed) {
    //
}

QString Translation::translation() const {
    return QString::fromStdString(response_->target.text);
}

QList<WordAlignment> Translation::alignments(qsizetype sourcePosFirst, qsizetype sourcePosLast) const {
    QList<WordAlignment> alignments;
    std::size_t sentenceIdxFirst, sentenceIdxLast, wordIdxFirst, wordIdxLast;

    if (!response_)
        return alignments;

    if (sourcePosFirst > sourcePosLast)
        std::swap(sourcePosFirst, sourcePosLast);

    std::size_t sourceOffsetFirst = ::positionToOffset(response_->source.text, sourcePosFirst);

    if (!findWordByByteOffset(response_->source.annotation, sourceOffsetFirst, sentenceIdxFirst, wordIdxFirst))
        return alignments;

    std::size_t sourceOffsetLast = ::positionToOffset(response_->source.text, sourcePosLast);

    if (!findWordByByteOffset(response_->source.annotation, sourceOffsetLast, sentenceIdxLast, wordIdxLast))
        return alignments;

    qDebug() << "From" <<sourcePosFirst << "to" << sourcePosLast << "==" << sentenceIdxFirst << ":" << wordIdxFirst << "to" << sentenceIdxLast << ":" << wordIdxLast;

    assert(sentenceIdxFirst <= sentenceIdxLast);
    assert(sentenceIdxFirst != sentenceIdxLast || wordIdxFirst <= wordIdxLast);
    assert(sentenceIdxLast < response_->alignments.size());

    for (std::size_t sentenceIdx = sentenceIdxFirst; sentenceIdx <= sentenceIdxLast; ++sentenceIdx) {
        assert(sentenceIdx < response_->alignments.size());
        std::size_t firstWord = sentenceIdx == sentenceIdxFirst ? wordIdxFirst : 0;
        std::size_t lastWord = sentenceIdx == sentenceIdxLast ? wordIdxLast : response_->source.numWords(sentenceIdx) - 1;
        for (marian::bergamot::Point const &point : response_->alignments[sentenceIdx]) {
            if (point.src >= firstWord && point.src <= lastWord) {
                auto span = response_->target.wordAsByteRange(sentenceIdx, point.tgt);
                WordAlignment alignment;
                alignment.begin = ::offsetToPosition(response_->target.text, span.begin);
                alignment.end = ::offsetToPosition(response_->target.text, span.end);
                alignment.prob = point.prob;
                alignments.append(alignment);
            }
        }
    }

    return alignments;
}

qsizetype Translation::findSourcePosition(qsizetype targetPos) const {
    std::size_t sentenceIdx, wordIdx;

    if (!response_)
        return -1;

    std::size_t targetOffset = ::positionToOffset(response_->target.text, targetPos);

    if (!findWordByByteOffset(response_->target.annotation, targetOffset, sentenceIdx, wordIdx))
        return -1;

    auto word = response_->target.word(sentenceIdx, wordIdx);
    // qDebug() << "Found position" << targetPos << "in word" << QString::fromUtf8(word.data(), word.size());

    marian::bergamot::Point best;
    best.prob = -1.0f;

    assert(sentenceIdx < response_->alignments.size());
    for (marian::bergamot::Point const &point : response_->alignments[sentenceIdx])
        if (point.tgt == wordIdx && point.prob > best.prob)
            best = point;

    if (best.prob < 0.0)
        return -1;

    auto sourceOffset = response_->source.wordAsByteRange(sentenceIdx, best.src).begin;
    // qDebug() << "Found opposite word at " << sourceOffset << "with word" << QString::fromUtf8(response_->source.word(sentenceIdx, best.src));

    return ::offsetToPosition(response_->source.text, sourceOffset);
}

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
                if (model) {
                    // Unload marian first (so we can delete loggers after that)
                    service.reset();

                    // We need to manually destroy the loggers, as marian doesn't do
                    // that but will complain when a new marian::Config tries to 
                    // initialise loggers with the same name.
                    spdlog::drop("general");
                    spdlog::drop("valid");

                    service.reset(new marian::bergamot::Service(MakeOptions(model->config_file, model->settings)));
                } else if (input) {
                    if (service) {
                        auto start = std::chrono::steady_clock::now(); // Time the translation
                        std::future<int> num_words = std::async(countWords, *input); // @TODO we're doing an unnecessary string copy here

                        marian::bergamot::ResponseOptions options;
                        options.alignment = true;
                        options.alignmentThreshold = 0.2f;

                        std::future<marian::bergamot::Response> responseFuture = service->translate(std::move(*input), options);
                        responseFuture.wait();
                        auto end = std::chrono::steady_clock::now();
                        // Calculate translation speed in terms of words per second
                        double words = num_words.get();
                        std::chrono::duration<double> elapsed_seconds = end-start;
                        int translationSpeed = std::ceil(words/elapsed_seconds.count()); // @TODO this could probably be done in the service in the future
                        emit translationReady(Translation(std::move(responseFuture.get()), translationSpeed));
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


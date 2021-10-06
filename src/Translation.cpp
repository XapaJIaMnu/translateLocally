#include "Translation.h"
#include "3rd_party/bergamot-translator/src/translator/response.h"
#include <QDebug>

namespace {

bool contains(marian::bergamot::ByteRange const &span, std::size_t offset) {
    return offset >= span.begin && offset < span.end;
}

bool findWordByByteOffset(marian::bergamot::Annotation const &annotation, std::size_t pos, std::size_t &sentenceIdx, std::size_t &wordIdx) {
    for (sentenceIdx = 0; sentenceIdx < annotation.numSentences(); ++sentenceIdx)
        if (annotation.sentence(sentenceIdx).end >= pos)
            break;

    if (sentenceIdx == annotation.numSentences())
        return false;
    
    for (wordIdx = 0; wordIdx < annotation.numWords(sentenceIdx); ++wordIdx)
        if (annotation.word(sentenceIdx, wordIdx).end >= pos)
            break;

    return wordIdx < annotation.numWords(sentenceIdx);
}

int offsetToPosition(std::string const &text, std::size_t offset) {
    if (offset > text.size())
        return -1;

    return QString::fromUtf8(text.data(), offset).size();
}

std::size_t positionToOffset(std::string const &text, int pos) {
    return QString::fromStdString(text).left(pos).toLocal8Bit().size();
}

marian::bergamot::AnnotatedText const &_source(marian::bergamot::Response const &response, Translation::Direction direction) {
    if (direction == Translation::source_to_translation)
        return response.source;
    else
        return response.target;
}

marian::bergamot::AnnotatedText const &_target(marian::bergamot::Response &response, Translation::Direction direction) {
    if (direction == Translation::source_to_translation)
        return response.target;
    else
        return response.source;
}

} // Anonymous namespace

Translation::Translation()
: response_(nullptr)
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

QList<WordAlignment> Translation::alignments(Direction direction, int sourcePosFirst, int sourcePosLast) const {
    QList<WordAlignment> alignments;
    std::size_t sentenceIdxFirst, sentenceIdxLast, wordIdxFirst, wordIdxLast;

    if (!response_)
        return alignments;

    if (sourcePosFirst > sourcePosLast)
        std::swap(sourcePosFirst, sourcePosLast);

    std::size_t sourceOffsetFirst = ::positionToOffset(::_source(*response_, direction).text, sourcePosFirst);
    if (!::findWordByByteOffset(::_source(*response_, direction).annotation, sourceOffsetFirst, sentenceIdxFirst, wordIdxFirst))
        return alignments;

    std::size_t sourceOffsetLast = ::positionToOffset(::_source(*response_, direction).text, sourcePosLast);
    if (!::findWordByByteOffset(::_source(*response_, direction).annotation, sourceOffsetLast, sentenceIdxLast, wordIdxLast))
        return alignments;


    assert(sentenceIdxFirst <= sentenceIdxLast);
    assert(sentenceIdxFirst != sentenceIdxLast || wordIdxFirst <= wordIdxLast);
    assert(sentenceIdxLast < response_->alignments.size());

    for (std::size_t sentenceIdx = sentenceIdxFirst; sentenceIdx <= sentenceIdxLast; ++sentenceIdx) {
        assert(sentenceIdx < response_->alignments.size());
        std::size_t firstWord = sentenceIdx == sentenceIdxFirst ? wordIdxFirst : 0;
        std::size_t lastWord = sentenceIdx == sentenceIdxLast ? wordIdxLast : ::_source(*response_, direction).numWords(sentenceIdx) - 1;
        for (marian::bergamot::Point const &point : response_->alignments[sentenceIdx]) {
            if (direction == source_to_translation ? (point.src >= firstWord && point.src <= lastWord) : (point.tgt >= firstWord && point.tgt <= lastWord)) {
                auto span = ::_target(*response_, direction).wordAsByteRange(sentenceIdx, direction == source_to_translation ? point.tgt : point.src);
                WordAlignment alignment;
                alignment.begin = ::offsetToPosition(::_target(*response_, direction).text, span.begin);
                alignment.end = ::offsetToPosition(::_target(*response_, direction).text, span.end);
                alignment.prob = point.prob;
                alignments.append(alignment);
            }
        }
    }

    std::sort(alignments.begin(), alignments.end(), [](WordAlignment const &a, WordAlignment const &b) {
        return a.begin < b.begin && a.prob > b.prob;
    });

    return alignments;
}

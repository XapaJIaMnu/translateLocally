#include "Translation.h"
#include "3rd_party/bergamot-translator/src/translator/response.h"
#include <QDebug>

namespace {

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
    if (offset > text.size())
        return -1;

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

    // qDebug() << "From" <<sourcePosFirst << "to" << sourcePosLast << "==" << sentenceIdxFirst << ":" << wordIdxFirst << "to" << sentenceIdxLast << ":" << wordIdxLast;

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
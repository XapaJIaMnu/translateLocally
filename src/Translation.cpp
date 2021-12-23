#include "Translation.h"
#include "3rd_party/bergamot-translator/src/translator/response.h"

namespace {

/**
 * Finds sentence and word index for a given byte offset in an annotated
 * string. These come out of marian::bergamot::Service.translate().
 */
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

/**
 * Converts byte offset into utf-8 aware character position.
 * Unicode checking blatantly stolen from https://stackoverflow.com/a/
 */
int offsetToPosition(std::string const &text, std::size_t offset) {
    // if offset is never bigger than text.size(), and we iterate based on
    // offset, *p will always be within [c_str(), c_str() + size())
    if (offset > text.size())
        offset = text.size();

    std::size_t pos = 0;
    for (char const *p = text.c_str(); offset > 0; --offset, p++) {
        if ((*p & 0xc0) != 0x80) // if is not utf-8 continuation character
            ++pos;
    }
    return pos;
}

/**
 * Other way around: converts utf-8 character position into a byte offset.
 */
std::size_t positionToOffset(std::string const &text, int pos) {
    char const *p = text.c_str(), *end = text.c_str() + text.size();
    // Continue for-loop while pos > 0 or while we're in a multibyte utf-8 char
    for (; p != end && (pos > 0 || (*p & 0xc0) == 0x80); p++) {
        if ((*p & 0xc0) != 0x80)
            --pos;
    }
    return p - text.c_str();
}

marian::bergamot::AnnotatedText const &_source(marian::bergamot::Response const &response, Translation::Direction direction) {
    if (direction == Translation::source_to_translation)
        return response.source;
    else
        return response.target;
}

marian::bergamot::AnnotatedText const &_target(marian::bergamot::Response const &response, Translation::Direction direction) {
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

QVector<WordAlignment> Translation::alignments(Direction direction, int sourcePosFirst, int sourcePosLast) const {
    QVector<WordAlignment> alignments;
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

    // Format:
    // response_->alignments[sentence:size_t][target token:size_t][source token:size_t] = probability:float

    auto append = [&](marian::bergamot::ByteRange const &span, float prob) {
        WordAlignment alignment;
        alignment.begin = ::offsetToPosition(::_target(*response_, direction).text, span.begin);
        alignment.end = ::offsetToPosition(::_target(*response_, direction).text, span.end);
        alignment.prob = prob;
        alignments.append(alignment);
    };

    for (std::size_t sentenceIdx = sentenceIdxFirst; sentenceIdx <= sentenceIdxLast; ++sentenceIdx) {
        assert(sentenceIdx < response_->alignments.size());
        std::size_t firstWord = sentenceIdx == sentenceIdxFirst ? wordIdxFirst : 0;
        std::size_t lastWord = sentenceIdx == sentenceIdxLast ? wordIdxLast : ::_source(*response_, direction).numWords(sentenceIdx) - 1;
        
        // If no alignments were provided by the model, this array will be empty
        if (response_->alignments[sentenceIdx].empty())
            continue;

        if (direction == Translation::source_to_translation) {
            assert(firstWord < response_->source.numWords(sentenceIdx));
            assert(lastWord <= response_->source.numWords(sentenceIdx));

            for (size_t t = 0; t < response_->target.numWords(sentenceIdx); ++t) {
                for (size_t s = firstWord; s <= lastWord; ++s) {
                    if (response_->alignments[sentenceIdx][t][s] >= 0.1f) // TODO top N or something?
                        append(response_->target.wordAsByteRange(sentenceIdx, t), response_->alignments[sentenceIdx][t][s]);
                }
            }
        } else {
            assert(firstWord < response_->target.numWords(sentenceIdx));
            assert(lastWord < response_->target.numWords(sentenceIdx));

            for (size_t t = firstWord; t <= lastWord; ++t) {
                for (size_t s = 0; s < response_->source.numWords(sentenceIdx); ++s) {
                    if (response_->alignments[sentenceIdx][t][s] >= 0.1f) // TODO top N or something?
                        append(response_->source.wordAsByteRange(sentenceIdx, s), response_->alignments[sentenceIdx][t][s]);
                }
            }
        }
    }

    // Sort by position (left to right), highest probability first.
    std::sort(alignments.begin(), alignments.end(), [](WordAlignment const &a, WordAlignment const &b) {
        return a.begin <= b.begin && a.prob > b.prob;
    });

    return alignments;
}

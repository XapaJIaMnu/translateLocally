#ifndef TRANSLATION_H
#define TRANSLATION_H
#include <QMetaType>
#include <QString>
#include <memory>

namespace marian {
    namespace bergamot {
        class Response;
    }
}

struct WordAlignment {
    std::size_t begin;
    std::size_t end;
    float prob;
};

class Translation {
private:
    std::shared_ptr<marian::bergamot::Response> response_;
    int speed_;
public:
    Translation();
    Translation(marian::bergamot::Response &&response, int speed);

    inline operator bool() const {
        return !!response_;
    }

    inline std::size_t wordsPerSecond() const {
        return speed_;
    }

    /**
     * Translation result
     */
    QString translation() const;

    /**
     * Looks up a list of character ranges and probability scores for words
     * aligning with the word at char pos `pos` in the source sentence. Returns
     * an empty list on failure.
     */
    QList<WordAlignment> alignments(qsizetype begin, qsizetype end) const;

    /**
     * Looks up the best cursor position for a source word when the position
     * of a target word is given. Returns -1 on failure.
     */
    qsizetype findSourcePosition(qsizetype pos) const;
};

Q_DECLARE_METATYPE(Translation)

#endif // TRANSLATION_H
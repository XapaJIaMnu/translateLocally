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

/**
 * Word alignment information, with information about a span in the translation
 * output as answer to querying which parts 'belong' to a span in the
 * translation input.
 */
struct WordAlignment {
    std::size_t begin; // Note: char offset in translation (not byte offset)
    std::size_t end;
    float prob; // Range: 0.0 - 1.0
};

/**
 * Wrapper around a translation response from the bergamot service. Hides that
 * interface from the rest of the Qt code, and provides utility functions to
 * access alignment information with character offsets instead of byte offsets.
 */
class Translation {
private:
    // Note: I would have liked unique_ptr, but that does not go well with
    // passing Translation objects through Qt signals/slots.
    std::shared_ptr<marian::bergamot::Response> response_;

    // @TODO this could probably be part of marian::bergamot::Response in the future
    int speed_;
public:
    Translation();
    Translation(marian::bergamot::Response &&response, int speed);

    /**
     * Bool operator to check whether this is an initialised translation or just
     * an empty object.
     */
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
    QList<WordAlignment> alignments(int begin, int end) const;

    /**
     * Looks up the best cursor position for a source word when the position
     * of a target word is given. Returns -1 on failure.
     */
    int findSourcePosition(int pos) const;
};

Q_DECLARE_METATYPE(Translation)

#endif // TRANSLATION_H
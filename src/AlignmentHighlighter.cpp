#include "AlignmentHighlighter.h"

AlignmentHighlighter::AlignmentHighlighter(QTextDocument *document)
: QSyntaxHighlighter(document) {
	//
}

void AlignmentHighlighter::setWordAlignment(QVector<WordAlignment> alignment) {
	alignment_ = alignment;
	rehighlight();
}

void AlignmentHighlighter::highlightBlock(QString const &text) {
	std::size_t offset = 0;

	if (previousBlockState() != -1)
		offset = previousBlockState();

	setCurrentBlockState(offset + text.length() + 1); // Add 1 for assumed "\n"
	                                                  // @TODO: safe assumption?

		
	// Optimisation: directly skip this while block if we know that both the start
	// and end of the whole span we have alignment information for is outside it.
	if (alignment_.empty() || alignment_.last().end < offset || alignment_.first().begin > offset + text.length())
		return;

	WordAlignment prev;
	for (auto const &word : alignment_) {
		// If this block is further down the page than this word, skip.
		if (word.end < offset)
			continue;

		// If this word is further down the page than this block, stop. alignment_
		// is guaranteed to be sorted by word.begin.
		if (word.begin > offset + text.length())
			break;

		// Skip spans that have the same overlap, but a lower probability. We can
		// do this because Translation::alignments() has a guaranteed order of
		// <word.begin low-to-high, word.prob high-to-low>.
		if (word.begin == prev.begin && word.prob < prev.prob)
			continue;
		
		QColor color(Qt::blue);
		color.setAlphaF(.5f * word.prob);
		
		QTextCharFormat format;
		format.setBackground(QBrush(color));
		
		setFormat(offset > word.begin ? 0 : word.begin - offset, word.end - word.begin, format);

		prev = word;
	}
}
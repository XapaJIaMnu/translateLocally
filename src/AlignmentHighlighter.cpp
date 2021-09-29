#include "AlignmentHighlighter.h"

AlignmentHighlighter::AlignmentHighlighter(QTextDocument *document)
: QSyntaxHighlighter(document) {
	//
}

void AlignmentHighlighter::setWordAlignment(QList<WordAlignment> alignment) {
	alignment_ = alignment;
	rehighlight();
}

void AlignmentHighlighter::highlightBlock(QString const &text) {
	std::size_t offset = 0;

	if (previousBlockState() != -1)
		offset = previousBlockState();

	setCurrentBlockState(offset + text.length() + 1); // Add 1 for assumed "\n"

	for (auto const &word : alignment_) {
		if (word.end < offset)
			continue;

		if (word.begin > offset + text.length())
			break;
		
		QColor color(Qt::blue);
		color.setAlphaF(.5f * word.prob);
		
		QTextCharFormat format;
		format.setBackground(QBrush(color));
		
		setFormat(offset > word.begin ? 0 : word.begin - offset, word.end - word.begin, format);
		// qDebug() << "from " << (word.begin - offset) << "to" << (word.end - offset) << ": " << word.prob;
	}
}
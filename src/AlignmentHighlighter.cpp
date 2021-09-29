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

	setCurrentBlockState(offset + text.length());

	for (auto const &word : alignment_) {
		if (word.end < offset)
			continue;

		if (word.begin > offset + text.length())
			break;
		
		QColor color(Qt::blue);
		color.setAlpha(255 * word.prob);
		
		QTextCharFormat format;
		format.setBackground(QBrush(color));
		
		setFormat(offset > word.begin ? 0 : word.begin - offset, word.end - word.begin, format);
	}

	qDebug() << "highlightBlock called with offset" << offset << "and" << text.length() << "chars";
}
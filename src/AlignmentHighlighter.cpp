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
	qDebug() << "highlightBlock called with" << text.length() << "chars";
}
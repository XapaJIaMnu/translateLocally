#ifndef ALIGNMENTHIGHLIGHTER_H
#define ALIGNMENTHIGHLIGHTER_H
#include "MarianInterface.h"
#include <QList>
#include <QSyntaxHighlighter>
#include <QDebug>

class AlignmentHighlighter: public QSyntaxHighlighter {
	Q_OBJECT

private:
	QList<WordAlignment> alignment_;

public:
	AlignmentHighlighter(QTextDocument *document);
	void setWordAlignment(QList<WordAlignment> alignment);
	void highlightBlock(QString const &text);
};

#endif // ALIGNMENTHIGHLIGHTER_H
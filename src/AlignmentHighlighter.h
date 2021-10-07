#ifndef ALIGNMENTHIGHLIGHTER_H
#define ALIGNMENTHIGHLIGHTER_H
#include "MarianInterface.h"
#include <QVector>
#include <QSyntaxHighlighter>
#include <QDebug>

class AlignmentHighlighter: public QSyntaxHighlighter {
	Q_OBJECT

private:
	QVector<WordAlignment> alignment_;

public:
	AlignmentHighlighter(QTextDocument *document);
	void setWordAlignment(QVector<WordAlignment> alignment);

protected:
	void highlightBlock(QString const &text);
};

#endif // ALIGNMENTHIGHLIGHTER_H
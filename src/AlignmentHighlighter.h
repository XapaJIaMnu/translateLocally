#ifndef ALIGNMENTHIGHLIGHTER_H
#define ALIGNMENTHIGHLIGHTER_H
#include "Translation.h"
#include <QTextDocument>
#include <QColor>
#include <QPointer>

class AlignmentHighlighter: public QObject {
	Q_OBJECT

private:
	QPointer<QTextDocument> document_;
	QColor color_;
	QVector<WordAlignment> alignments_;

public:
	AlignmentHighlighter(QObject *parent = nullptr);
	~AlignmentHighlighter();
	
	void setDocument(QTextDocument *document);
	void setColor(QColor color);
	void highlight(QVector<WordAlignment> alignment);
private:
	void render(QVector<WordAlignment> alignment);
};

#endif // ALIGNMENTHIGHLIGHTER_H
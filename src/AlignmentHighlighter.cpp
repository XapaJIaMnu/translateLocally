#include "AlignmentHighlighter.h"
#include "Translation.h"
#include <QTextBlock>

AlignmentHighlighter::AlignmentHighlighter(QObject *parent)
: QObject(parent)
, color_(Qt::blue) {
	
}

AlignmentHighlighter::~AlignmentHighlighter() {
	// Remove any left-over highlights when this highlighter is destroyed
	highlight(QVector<WordAlignment>());
}

void AlignmentHighlighter::setColor(QColor color) {
	color_ = color;
	highlight(alignments_);
}

void AlignmentHighlighter::setDocument(QTextDocument *document) {
	// no-op if this is already the current document
	if (document == document_.data())
		return;

	highlight(QVector<WordAlignment>()); // clear highlights from old document
	document_ = document;
}

void AlignmentHighlighter::highlight(QVector<WordAlignment> alignments) {
	render(alignments);
	alignments_ = alignments;
}

void AlignmentHighlighter::render(QVector<WordAlignment> alignments) {
	if (!document_)
		return;

	QTextBlock block = document_->end();

	auto alignment = alignments.cbegin();

	// Find first block to touch based on old alignments (we touch them to clear
	// the old formatting)
	if (!alignments_.empty())
		block = document_->findBlock(alignments_.first().begin);

	// Find the first block according to new alignments. See which one is earlier.
	if (alignment != alignments.cend())
		if (!block.isValid() || alignment->begin < block.position())
			block = document_->findBlock(alignment->begin);

	// Move through all blocks until the end of the document to figure out whether
	// alignments fall inside them.
	// TODO: early stopping based on max(alignments_.last().end alignments.last().end)
	for (; block != document_->end(); block = block.next()) {
		QTextLayout *layout = block.layout();
		QList<QTextFormat> formats;
		bool dirty = false;

		QVector<QTextLayout::FormatRange> ranges;

		// Remove any old formatting left by previous highlighting?
		if (!layout->formats().empty())
				dirty = true;

		// If we're out of alignments, or ff this block is behind the current
		// alignment iterator, stop and move to the next block iteration.
		// Note: assumes a single WordAlignment never spans across QTextBlock.
		for (; alignment != alignments.cend() && alignment->begin < block.position() + block.length(); ++alignment) {
			QColor color(color_);
			color.setAlphaF(.5f * alignment->prob);

			QTextCharFormat format;
			format.setBackground(QBrush(color));

			QTextLayout::FormatRange range;
			range.format = format;
			range.start = alignment->begin - block.position();
			range.length = alignment->end - alignment->begin;

			ranges.append(range);
		}

		if (dirty || !ranges.empty()) {
				layout->setFormats(ranges);
				document_->markContentsDirty(block.position(), block.length());
		}
	}

	// Note: Not necessarily a bug! e.g. empty document on initialisation.
	// assert(alignment == alignments.cend());
}
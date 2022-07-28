#include "FilterTableView.h"
#include <QFrame>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QHBoxLayout>
#include <QShortcut>
#include <QKeySequence>


FilterTableView::FilterTableView(QWidget *parent)
: QTableView(parent)
, frame_(new QFrame(this))
, field_(new QLineEdit())
, closeButton_(new QPushButton()) {
	// Give the frame a small border and a background (defaults to the window background which is fine)
	frame_->setFrameShape(QFrame::Box);
	frame_->setAutoFillBackground(true);

	// Changing text in the field immediately is forwarded as filterTextChanged
	connect(field_, &QLineEdit::textChanged, this, &FilterTableView::filterTextChanged);

	// Close button calls hideFilterFrame
	closeButton_->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
	connect(closeButton_, &QPushButton::clicked, this, &FilterTableView::hideFilterFrame);

	// Put the text field and the close button in a horizontal layout, text field may grow
	auto layout = new QHBoxLayout(frame_);
	layout->setContentsMargins(2, 2, 2, 2);
	layout->addWidget(field_, 1);
	layout->addWidget(closeButton_);

	// ctrl + f shows & focusses the search frame
	auto searchShortcut = new QShortcut(QKeySequence::Find, this);
	connect(searchShortcut, &QShortcut::activated, this, &FilterTableView::showFilterFrame);

	// escape hides the search frame
	auto closeShortcut = new QShortcut(QKeySequence::Cancel, frame_);
	connect(closeShortcut, &QShortcut::activated, this, &FilterTableView::hideFilterFrame);

	// search frame hidden by default.
	frame_->hide();
}

QString FilterTableView::filterText() const {
	return frame_->isVisible() ? field_->text() : QString();
}

void FilterTableView::paintEvent(QPaintEvent *event) {
	QTableView::paintEvent(event);

	int margin = 8; // Keeping space for scrollbars? We could query those spaces
	int width = geometry().width() - 2 * margin; // entire width
	int height = frame_->sizeHint().height(); // height of the contents
	int x = margin;
	int y = geometry().height() - height - margin; // fix to bottom of table
	
	frame_->setGeometry(QRect(x, y, width, height));
}

void FilterTableView::showFilterFrame() {
	frame_->show();
	field_->setFocus(Qt::PopupFocusReason);

	if (!field_->text().isEmpty()) {
		// Select all text; default behaviour is then exactly the same as if you
		// started with an empty text field. But you have to option to deselect
		// (e.g. right arrow key) and continue filtering where you left off.
		field_->selectAll();

		emit filterTextChanged(field_->text());
	}
}

void FilterTableView::hideFilterFrame() {
	frame_->hide();

	if (!field_->text().isEmpty())
		emit filterTextChanged(QString());
}

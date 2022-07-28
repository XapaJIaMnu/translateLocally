#pragma once
#include <QTableView>

class QFrame;
class QLineEdit;
class QPushButton;

/**
 * QTableView with search field that pops up when you press ctrl + f.
 */
class FilterTableView : public QTableView
{
	Q_OBJECT

public:
	FilterTableView(QWidget *parent = nullptr);

	/**
	 * Filter text. Empty if filter frame is hidden.
	 */
	QString filterText() const;

signals:
	/**
	 * Emits filter text changes while typing. Also emits empty text when filter
	 * frame is hidden again, and previous filter text when it is re-opened.
	 */
  void filterTextChanged(QString text);

public slots:
	/**
	 * Shows filter frame, focusses the text field, and if there is already text
	 * in there from a previous session it will emit `filterTextChanged()`.
	 */
	void showFilterFrame();
  
  /**
	 * Hides frame and sets filterText to empty string.
	 */
  void hideFilterFrame();

protected:
	/**
	 * Override from parent to update frame positioning
	 */
	virtual void paintEvent(QPaintEvent *event);

private:
	/**
	 * Frame that floats over the bottom of the table
	 */
  QFrame *frame_;

  /**
   * Text field that contains filter text. Any input changes are rebroadcasted
   * through `filterTextChanged()`.
   */
  QLineEdit *field_;

  /**
   * Button to close filter frame. Can emit `filterTextChanged()` with empty
   * string if filter text was previously not empty.
   */
  QPushButton *closeButton_;
};

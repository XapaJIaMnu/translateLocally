#ifndef ALIGNMENTWORKER_H
#define ALIGNMENTWORKER_H
#include <QObject>
#include <QMutex>
#include <QSemaphore>
#include <memory>
#include <thread>
#include "Translation.h"

class AlignmentWorker : public QObject {
	Q_OBJECT

private:
	struct Request {
		Translation translation;
		Translation::Direction direction;
		int begin;
		int end;
	};

	std::unique_ptr<Request> pendingRequest_;
	QSemaphore commandIssued_;
	QMutex lock_;

	std::thread worker_;

public:
	AlignmentWorker(QObject *parent = nullptr);
	~AlignmentWorker();
	void query(Translation const &translation, Translation::Direction direction, int begin, int end);

signals:
	void ready(QList<WordAlignment> alignments);
};

#endif

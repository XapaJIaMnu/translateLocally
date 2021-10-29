#include "AlignmentWorker.h"
#include <QMutexLocker>

AlignmentWorker::AlignmentWorker(QObject *parent)
: QObject(parent)
, pendingRequest_(nullptr) {
	worker_ = std::thread([&]() {
		while (true) {
			std::unique_ptr<Request> request;

			commandIssued_.acquire();

			{
				QMutexLocker locker(&lock_);
				std::swap(request, pendingRequest_);
			}

			if (!request)
					break;

			QVector<WordAlignment> alignments;

			if (request->translation)
				alignments = request->translation.alignments(request->direction, request->begin, request->end);
			
			emit ready(alignments, request->direction);
		}
	});
}

AlignmentWorker::~AlignmentWorker() {
	{
		QMutexLocker locker(&lock_);
		pendingRequest_.reset();
	}
	commandIssued_.release();
	worker_.join();
}

void AlignmentWorker::query(Translation const &translation, Translation::Direction direction, int begin, int end) {
	std::unique_ptr<Request> request(new Request{translation, direction, begin, end});

	{
		QMutexLocker locker(&lock_);
		std::swap(request, pendingRequest_);
	}

	if (!request)
		commandIssued_.release();
}

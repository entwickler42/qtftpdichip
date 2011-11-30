#ifndef QTFTDICHIOLIST_H
#define QTFTDICHIOLIST_H

#include "qtftdichiplist.h"

#include <QList>
#include <QMutex>

#ifdef QTFTDICHIP_LIBFTDI
#include "qtftdichiplistworker_libftdi.h"
#endif

#ifdef QTFTDICHIP_D2XX
#include "qtftdichiplistworker_d2xx.h"
#endif

class QtFtdiChipListPrivate
		: public QObject
{
	Q_OBJECT

public:
	QtFtdiChipListPrivate(QtFtdiChipList* parent) :
		QObject(parent),
		last_error(0),
		chip_list(parent),
		worker_thread(new QtFtdiChipListWorker(this))
	{
		connect(worker_thread, SIGNAL(started()),  this, SLOT(onWorkerStarted()),  Qt::QueuedConnection);
		connect(worker_thread, SIGNAL(finished()), this, SLOT(onWorkerFinished()), Qt::QueuedConnection);
		connect(worker_thread, SIGNAL(error(int)), this, SLOT(onWorkerError(int)), Qt::QueuedConnection);
		connect(worker_thread, SIGNAL(entry(QtFtdiChipInformation)), this, SLOT(onWorkerEntry(QtFtdiChipInformation)), Qt::QueuedConnection);
	}

	~QtFtdiChipListPrivate()
	{}

	int last_error;
	QtFtdiChipList* chip_list;
	QList<QtFtdiChipInformation> ls_info;

	QtFtdiChipListWorker* worker_thread;
	QMutex mutex_fetch;

private slots:	
	void onWorkerStarted()
	{
		ls_info.clear();
		emit chip_list->started();
	}
	
	void onWorkerFinished()
	{
		emit chip_list->finished();
	}

	void onWorkerError(int e)
	{
		last_error = e;
		emit chip_list->error(e);
	}

	void onWorkerEntry(QtFtdiChipInformation info)
	{
		ls_info.push_back(info);
		emit chip_list->entry(info);
	}
};


#endif

#include "qtftdichiplist_p.h"

static int QtFtdiChipInformation_MetaType_ID = qRegisterMetaType<QtFtdiChipInformation>("QtFtdiChipInformation");

QtFtdiChipList::QtFtdiChipList(QObject* parent) :
	QObject(parent),
	d(new QtFtdiChipListPrivate(this))
{}

QtFtdiChipList::~QtFtdiChipList()
{}

int QtFtdiChipList::lastError() const
{
	return d->last_error;
}

int QtFtdiChipList::count() const
{
	return d->ls_info.count();
}

const QtFtdiChipInformation& QtFtdiChipList::at(int index) const
{
	Q_ASSERT_X( index < d->ls_info.count(), Q_FUNC_INFO, qPrintable(tr("index %1 is out of bounds").arg(index)) );
	return d->ls_info.at(index);
}

QtFtdiChipInformation& QtFtdiChipList::operator[](int index) const
{
	Q_ASSERT_X( index < d->ls_info.count(), Q_FUNC_INFO, qPrintable(tr("index %1 is out of bounds").arg(index)) );
	return d->ls_info[index];
}

bool QtFtdiChipList::fetch()
{
	QMutexLocker lock(&d->mutex_fetch);

	if( d->worker_thread->isRunning() )
		return false;

	d->worker_thread->start(QThread::HighPriority);
	return true;
}


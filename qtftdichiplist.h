#ifndef QTFTDICHIPLIST_H
#define QTFTDICHIPLIST_H

#include "qtftdichip_global.h"

#include <QObject>
#include <QtGlobal>
#include <QMetaType>


class QtFtdiChipListPrivate;

class QTFTDICHIPSHARED_EXPORT QtFtdiChipInformation
{
	friend class QtFtdiChip;
	friend class QtFtdiChipListWorker;

public:
	qint32 id() const
	{ return m_id; }

	QString serial() const
	{ return m_serial; }

	QString description() const
	{ return m_description; }

private:
	qint32 m_id;
	QString m_serial;
	QString m_description;
};

Q_DECLARE_METATYPE(QtFtdiChipInformation);


class QtFtdiChipList :
		public QObject
{
	Q_OBJECT

	friend class QtFtdiChipListPrivate;

signals:	
	void started();
	void finished();	
	void error(int);	
	void entry(QtFtdiChipInformation);

public:
	QtFtdiChipList(QObject* parent = 0);
	virtual ~QtFtdiChipList();
	
	int lastError() const;	
	int count() const;	
	const QtFtdiChipInformation& at(int index) const;	
	QtFtdiChipInformation& operator[](int index) const;

public slots:
	bool fetch();

private:
	QtFtdiChipListPrivate* d;
};
#endif

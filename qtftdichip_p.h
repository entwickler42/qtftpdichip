#ifndef QTFTDICHIP_P_H
#define QTFTDICHIP_P_H

#include "qtftdichip.h"

#include <QMutex>
#include <QWaitCondition>

class QtFtdiChipReader;
class QtFtdiChipWriter;

#ifdef QTFTDICHIP_LIBFTDI
#include <ftdi.h>
typedef struct ftdi_context LL_HANDLE_TYPE;
#endif

#ifdef QTFTDICHIP_D2XX
#include <windows.h>
#include <ftd2xx.h>
typedef FT_HANDLE LL_HANDLE_TYPE;
#endif

#define WAIT_TIMEOUT_TERMINATE 500
#define WAIT_TIMEOUT_RX 100
#define WAIT_TIMEOUT_TX 100
#define LOCK_TIMEOUT_TX 100

class QtFtdiChipPrivate :
		public QObject
{
	Q_OBJECT

public:
	QtFtdiChipPrivate(QtFtdiChip* parent) :
		QObject(parent),
		ftdichip(parent),
		run_reader(true),
		run_writer(true),
		last_error(0),
		reader_thread(0),
		writer_thread(0)
	{}

	virtual ~QtFtdiChipPrivate()
	{}

	QtFtdiChip* ftdichip;

	bool open_by_index;
	bool run_reader; 
	bool run_writer; 

	int last_error;
	int index;

	QString serial;

	LL_HANDLE_TYPE handle;

	QMutex mutex_handle;
	QMutex mutex_rx_buffer;
	QMutex mutex_tx_buffer;
	QMutex mutex_ready_read;
	QMutex mutex_bytes_written;

	QByteArray buffer_rx; 
	QByteArray buffer_tx; 

	QWaitCondition cond_ready_read;
	QWaitCondition cond_tx_available;
	QWaitCondition cond_bytes_written;

	QtFtdiChipReader* reader_thread;
	QtFtdiChipWriter* writer_thread;

	void createIOThreads();
	void destroyIOThreads();
	bool waitForCondition(QMutex* mutex, QWaitCondition* cond, int msecs);

private slots:
	void readyRead()
	{ emit ftdichip->readyRead(); }

	void bytesWritten(qint64 n)
	{ emit ftdichip->bytesWritten(n);}
};

#endif 

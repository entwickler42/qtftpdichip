#ifndef QTFTDICHIPREADER_LIBFTDI_H
#define QTFTDICHIPREADER_LIBFTDI_H

#include "qtftdichip_p.h"

#include <QThread>

class QtFtdiChipReader :
		public QThread
{
	Q_OBJECT

signals:
	void readyRead();

public:
	QtFtdiChipReader(AFtdiChipPrivate* data) :
		QThread(0),
		d(data)
	{
		setTerminationEnabled(true);
		d->run_reader = true;
	}

	virtual ~QtFtdiChipReader()
	{
		d->run_reader = false;
		yieldCurrentThread();

		if( isRunning() && !wait(WAIT_TIMEOUT_TERMINATE) ){
			qDebug("%s: termination required!", Q_FUNC_INFO);
			terminate();
		}
	}

protected:
	void run()
	{
		unsigned int chunk_size;

		d->mutex_handle.lock();
		ftdi_read_data_get_chunksize(&d->handle, &chunk_size);
		d->mutex_handle.unlock();

		unsigned char *buf = new unsigned char[chunk_size];

		do{
			d->mutex_handle.lock();
			int status = ftdi_read_data(&d->handle, buf, chunk_size);
			d->mutex_handle.unlock();
			if( status > 0 ){
				d->mutex_rx_buffer.lock();
				d->buffer_rx.append((const char*)buf, status);
				d->mutex_rx_buffer.unlock();

				d->mutex_ready_read.lock();
				d->cond_ready_read.wakeAll();
				d->mutex_ready_read.unlock();

				emit readyRead();
			}else{
				usleep(20);
			}

		}while( d->run_reader );
		delete buf;
	}

private:
	AFtdiChipPrivate* d;
};

class QtFtdiChipWriter :
		public QThread
{
	Q_OBJECT

signals:
	void bytesWritten(qint64);

public:
	QtFtdiChipWriter(AFtdiChipPrivate* data) :
		QThread(0),
		d(data)
	{
		setTerminationEnabled(true);
		d->run_writer = true;
	}

	virtual ~QtFtdiChipWriter()
	{
		d->run_writer = false;
		yieldCurrentThread();

		if( isRunning() && !wait(WAIT_TIMEOUT_TERMINATE) ){
			qDebug("%s: termination required!", Q_FUNC_INFO);
			terminate();
		}
	}

protected:
	void run()
	{
		do{
			if( !d->mutex_tx_buffer.tryLock(LOCK_TIMEOUT_TX) )
				continue;
			if( !d->buffer_tx.count() && !d->cond_tx_available.wait(&d->mutex_tx_buffer, WAIT_TIMEOUT_TX) ){
				d->mutex_tx_buffer.unlock();
				continue;
			}

			QByteArray buf = d->buffer_tx;
			d->buffer_tx.clear();
			d->mutex_tx_buffer.unlock();

			d->mutex_handle.lock();
			int status = ftdi_write_data(&d->handle, (unsigned char*)buf.data(), buf.size());
			d->mutex_handle.unlock();

			d->mutex_bytes_written.lock();
			d->cond_bytes_written.wakeAll();
			d->mutex_bytes_written.unlock();

			emit bytesWritten(status);

		}while( d->run_writer );
	}

private:
	AFtdiChipPrivate* d;
};

#endif 

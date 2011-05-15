#ifndef QTFTDICHIPREADER_D2XX_H
#define QTFTDICHIPREADER_D2XX_H

#include "qtftdichip_p.h"
#include <QThread>

#if defined(Q_OS_UNIX)
#include <errno.h>
#include <pthread.h>
#endif

#ifdef DEBUG_D2XX_CALL
#define D2XX_CALL(fn, ...) { \
	QTime d2xx_call_stop_watch = QTime::currentTime(); \
	d2xx_status = fn(__VA_ARGS__); \
	qDebug("D2XX CALL: %s (%s) == %i (%i ms)", # fn, # __VA_ARGS__, d2xx_status, d2xx_call_stop_watch.elapsed() ); }
#else
#define D2XX_CALL(fn, ...) d2xx_status = fn(__VA_ARGS__)
#endif


class QtFtdiChipReader :
		public QThread
{
	Q_OBJECT

signals:
	void readyRead();

public:
	QtFtdiChipReader(QtFtdiChipPrivate* data) :
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
#if defined(Q_OS_WIN32)
	void run()
	{
		FT_STATUS d2xx_status;
		HANDLE hEvent = CreateEvent(0, false, false, TEXT(""));

		d->mutex_handle.lock();
		D2XX_CALL( FT_SetEventNotification, d->handle, FT_EVENT_RXCHAR, hEvent );
		d->mutex_handle.unlock();

		while( d->run_reader ){
			if( WaitForSingleObject(hEvent, WAIT_TIMEOUT_RX) == WAIT_OBJECT_0 ){
				readAllBytes();
			}
		}

		d->mutex_handle.lock();
		D2XX_CALL(FT_SetEventNotification, d->handle, 0, hEvent);
		d->mutex_handle.unlock();
	}
#else
	void run()
	{
		EVENT_HANDLE hEvent;
		pthread_mutex_init(&hEvent.eMutex, 0);
		pthread_cond_init(&hEvent.eCondVar, 0);
		
		int read_timeout = 100;

		while( d->run_reader ){
			struct timeval time;
			struct timespec wait_time;
			bool data_available;
			FT_STATUS state = FT_SetEventNotification(d->handle, FT_EVENT_RXCHAR|FT_EVENT_MODEM_STATUS, (void*)&hEvent);

			gettimeofday(&time, 0);
			long nsec = 1000 * (read_timeout * 1000 + time.tv_usec);
			wait_time.tv_sec  = time.tv_sec + nsec / 1000000000;
			wait_time.tv_nsec = nsec % 1000000000;

			pthread_mutex_lock(&hEvent.eMutex);
			data_available = (ETIMEDOUT != pthread_cond_timedwait(&hEvent.eCondVar, &hEvent.eMutex, &wait_time));
			pthread_mutex_unlock(&hEvent.eMutex);

			if( data_available ){
				readAllBytes();
			}
			state = FT_SetEventNotification(d->handle, 0, &hEvent);
		}
		pthread_cond_destroy(&hEvent.eCondVar);
		pthread_mutex_destroy(&hEvent.eMutex);
	}
#endif

private:
	QtFtdiChipPrivate* d;

	inline void readAllBytes()
	{
		FT_STATUS d2xx_status;
		DWORD event=0, crx=0, nrx=0, ntx=0;

		d->mutex_handle.lock();
		D2XX_CALL(FT_GetStatus, d->handle, &nrx, &ntx, &event);

		if( nrx > 0 ){
			char* buf = new char[nrx];
			D2XX_CALL(FT_Read, d->handle, buf, nrx, &crx);
			d->mutex_handle.unlock();

			if( d2xx_status == FT_OK ){
				d->mutex_rx_buffer.lock();
				d->buffer_rx.append(buf, crx);
				d->mutex_rx_buffer.unlock();
				
				d->mutex_ready_read.lock();
				d->cond_ready_read.wakeAll();
				d->mutex_ready_read.unlock();

				emit readyRead();
			}
			delete buf;
		}else{
			d->mutex_handle.unlock();
		}
	}
};

class QtFtdiChipWriter :
		public QThread
{
	Q_OBJECT

signals:
	void bytesWritten(qint64);

public:
	QtFtdiChipWriter(QtFtdiChipPrivate* data) :
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
		while( d->run_writer ){
			if( !d->mutex_tx_buffer.tryLock(LOCK_TIMEOUT_TX) ){
				continue;
			}
			if( !d->buffer_tx.count() && !d->cond_tx_available.wait(&d->mutex_tx_buffer, WAIT_TIMEOUT_TX) ){
				d->mutex_tx_buffer.unlock();
				continue;
			}

			QByteArray buf = d->buffer_tx;
			d->buffer_tx.clear();
			d->mutex_tx_buffer.unlock();

			DWORD ntx;
			FT_STATUS d2xx_status;

			d->mutex_handle.lock();
			D2XX_CALL(FT_Write, d->handle, buf.data(), buf.size(), &ntx);
			d->mutex_handle.unlock();

			d->mutex_bytes_written.lock();
			d->cond_bytes_written.wakeAll();
			d->mutex_bytes_written.unlock();

			emit bytesWritten(ntx);
		}
	}

private:
	QtFtdiChipPrivate* d;
};

#endif

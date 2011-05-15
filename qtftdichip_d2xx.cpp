#include "qtftdichip_p.h"

#include <QTime>
#include <QCoreApplication>

#include "qtftdichipworker_d2xx.h"

#define D2XX_CALL_RETURN_BOOLEAN(fn, ...) \
	QMutexLocker locker(&d->mutex_handle); \
	FT_STATUS d2xx_status; \
	D2XX_CALL(fn, d->handle, __VA_ARGS__); \
	if( d2xx_status != FT_OK ){ \
	setErrorString(tr("%1: failed! (%2)").arg(Q_FUNC_INFO).arg(d2xx_status)); \
	qDebug("%s: failed! (%i)", Q_FUNC_INFO, (int)d2xx_status); \
	return false; \
	} \
	return true

#define D2XX_CALL_RETURN_BOOLEAN0(fn) \
	QMutexLocker locker(&d->mutex_handle); \
	FT_STATUS d2xx_status; \
	D2XX_CALL(fn, d->handle); \
	if( d2xx_status != FT_OK ){ \
	setErrorString(tr("%1: failed! (%2)").arg(Q_FUNC_INFO).arg(d2xx_status)); \
	qDebug("%s: failed! (%i)", Q_FUNC_INFO, (int)d2xx_status); \
	return false; \
	} \
	return true


void QtFtdiChipPrivate::createIOThreads()
{
	writer_thread = new QtFtdiChipWriter(this);
	reader_thread = new QtFtdiChipReader(this);
	connect(writer_thread, SIGNAL(bytesWritten(qint64)), this, SLOT(bytesWritten(qint64)));
	connect(reader_thread, SIGNAL(readyRead()), this, SLOT(readyRead()));
	writer_thread->start(QThread::HighestPriority);
	reader_thread->start(QThread::HighestPriority);
	QThread::yieldCurrentThread();
}

void QtFtdiChipPrivate::destroyIOThreads()
{
	delete writer_thread;
	delete reader_thread;
	reader_thread = 0;
	writer_thread = 0;
}

bool QtFtdiChipPrivate::waitForCondition(QMutex* mutex, QWaitCondition* cond, int msecs)
{
	QTime stop_watch = QTime::currentTime();

	bool status = false;
	if( mutex->tryLock(msecs) ){
		msecs -= stop_watch.elapsed();
		if( msecs > 0 ){
			status = cond->wait(mutex, msecs);
		}
		mutex->unlock();
	}

	return status;
}

QtFtdiChip::QtFtdiChip(const QString& serial, QObject *parent) :
	QIODevice(parent),
	d(new QtFtdiChipPrivate(this))
{
	d->open_by_index = false;
	d->serial = serial;
}

QtFtdiChip::QtFtdiChip(int index, QObject *parent) :
	QIODevice(parent),
	d(new QtFtdiChipPrivate(this))
{
	d->open_by_index = true;
	d->index = index;
}

QtFtdiChip::~QtFtdiChip()
{
	close();
}

bool QtFtdiChip::isSequential() const
{
	return false;
}

bool QtFtdiChip::open(OpenMode mode)
{
	FT_STATUS d2xx_status = FT_OK;

	if( isOpen() )
		return true;

	if( d->open_by_index ){
		D2XX_CALL( FT_Open, d->index, &d->handle );
	}else{
		D2XX_CALL( FT_OpenEx, (PVOID)qPrintable(d->serial), FT_OPEN_BY_SERIAL_NUMBER, &d->handle);
	}

	if (d2xx_status != FT_OK){
		setErrorString(tr("%1: failed (%2) to open the device! ").arg(Q_FUNC_INFO).arg(d2xx_status));
		return false;
	}

	d->createIOThreads();

	return QIODevice::open(mode);
}

void QtFtdiChip::close()
{
	if( !isOpen() )
		return;

	d->destroyIOThreads();
	d->mutex_handle.lock();
	FT_STATUS d2xx_status;
	D2XX_CALL(FT_Close, d->handle);
	d->mutex_handle.unlock();
	if( d2xx_status == FT_OK ){
		QIODevice::close();
	}else{
		setErrorString(tr("%1: failed (%2) to close the device! ").arg(Q_FUNC_INFO).arg(d2xx_status));
		d->createIOThreads();
	}
}

qint64 QtFtdiChip::readData(char *data, qint64 maxlen)
{
	QMutexLocker lock(&d->mutex_rx_buffer);

	maxlen = qMin(maxlen, qint64(d->buffer_rx.count()));
	QByteArray rx = d->buffer_rx.left(maxlen);
	d->buffer_rx.remove(0, maxlen);
	memcpy(data, rx.data(), maxlen);

	return maxlen;
}

qint64 QtFtdiChip::writeData(const char *data, qint64 len)
{
	QMutexLocker lock(&d->mutex_tx_buffer);

	d->buffer_tx.append(data, len);
	d->cond_tx_available.wakeAll();

	return len;
}

qint64 QtFtdiChip::bytesAvailable() const
{
	QMutexLocker lock(&d->mutex_rx_buffer);
	return d->buffer_rx.count();
}

qint64 QtFtdiChip::bytesToWrite() const
{
	QMutexLocker lock(&d->mutex_tx_buffer);
	return d->buffer_tx.count();
}

bool QtFtdiChip::waitForReadyRead(int msecs)
{
	return d->waitForCondition(&d->mutex_ready_read, &d->cond_ready_read, msecs);
}

bool QtFtdiChip::waitForBytesWritten(int msecs)
{
	return d->waitForCondition(&d->mutex_bytes_written, &d->cond_bytes_written, msecs);
}

bool QtFtdiChip::addVidPid(quint32 vid, quint32 pid)
{
#ifdef Q_OS_UNIX
	if( FT_SetVIDPID(DWORD(vid), DWORD(pid)) != FT_OK ){
		return false;
	}
#else
	Q_UNUSED(vid);
	Q_UNUSED(pid);
	qWarning("addVidPid(...) is not available on this platform");
#endif
	return true;
}

bool QtFtdiChip::resetPort()
{
#ifdef Q_OS_UNIX
	qWarning("FT_ResetPort(...) is not available on this platform");
	return false;
#else
	D2XX_CALL_RETURN_BOOLEAN0(FT_ResetPort);
#endif
}

bool QtFtdiChip::resetDevice()
{
#ifdef Q_OS_UNIX
	qWarning("FT_ResetDevice(...) is not available on this platform");
	return false;
#else
	D2XX_CALL_RETURN_BOOLEAN0(FT_ResetDevice);
#endif
}

bool QtFtdiChip::purge(bool tx, bool rx)
{
	QMutexLocker lock_handle(&d->mutex_handle);

	FT_STATUS d2xx_status;
	DWORD channel = 0x0;
	bool success = true;

	if( rx ) {
		channel |= FT_PURGE_RX;
		d->mutex_rx_buffer.lock();
		d->buffer_rx.clear();
		d->mutex_rx_buffer.unlock();
	}

	if( tx ) {
		channel |= FT_PURGE_TX;
		d->mutex_tx_buffer.lock();
		d->buffer_tx.clear();
		d->mutex_tx_buffer.unlock();
	}
	D2XX_CALL(FT_Purge, d->handle, channel);
	if( d2xx_status != FT_OK ){
		setErrorString(tr("%1: failed! (%2)").arg(Q_FUNC_INFO).arg(d2xx_status));
		success = false;
	}

	return success;
}

bool QtFtdiChip::setTimeouts(quint32 readTimeout, quint32 writeTimeout)
{
	D2XX_CALL_RETURN_BOOLEAN(FT_SetTimeouts, readTimeout, writeTimeout);
}

bool QtFtdiChip::setResetPipeRetryCount(quint32 count)
{
	D2XX_CALL_RETURN_BOOLEAN(FT_SetResetPipeRetryCount, count);
}

bool QtFtdiChip::setFlowControl(quint16 flowControl, uchar xon, uchar xoff)
{
	D2XX_CALL_RETURN_BOOLEAN(FT_SetFlowControl, flowControl, xon, xoff);
}

bool QtFtdiChip::setDataCharacteristics(uchar wordlength, uchar stopbits, uchar parity)
{
	D2XX_CALL_RETURN_BOOLEAN(FT_SetDataCharacteristics, wordlength, stopbits, parity);
}

bool QtFtdiChip::setBaudRate(quint32 bautrate)
{
	D2XX_CALL_RETURN_BOOLEAN(FT_SetBaudRate, bautrate);
}

bool QtFtdiChip::setChars(uchar eventCh, uchar eventChEn, uchar errorCh, uchar errorChEn)
{
	D2XX_CALL_RETURN_BOOLEAN(FT_SetChars, eventCh, eventChEn, errorCh, errorChEn);
}

bool QtFtdiChip::setUSBParameters(quint32 inTransferSize, quint32 outTransferSize)
{
	D2XX_CALL_RETURN_BOOLEAN(FT_SetUSBParameters, inTransferSize, outTransferSize);
}

bool QtFtdiChip::setLatencyTime(uchar latency)
{
	D2XX_CALL_RETURN_BOOLEAN(FT_SetLatencyTimer, latency);
}

bool QtFtdiChip::getDeviceInfo(QtFtdiChipInformation* info)
{
	QMutexLocker locker(&d->mutex_handle);
	FT_STATUS d2xx_status;
	FT_DEVICE ftDevice;
	DWORD devid;
	char desc[64];
	char serial[16];

	D2XX_CALL(FT_GetDeviceInfo, d->handle, &ftDevice, &devid, serial, desc, 0);
	if( d2xx_status != FT_OK ){
		setErrorString(tr("%1: failed! (%2)").arg(Q_FUNC_INFO).arg(d2xx_status));
		return false;
	}

	info->m_id = devid;
	info->m_serial = QString(serial);
	info->m_description = QString(desc);

	return true;
}

bool QtFtdiChip::readEE(quint32 offset, quint16* value)
{
	FT_STATUS d2xx_status;
	D2XX_CALL(FT_ReadEE, d->handle, offset, value);
	return FT_OK == d2xx_status;
}

bool QtFtdiChip::writeEE(quint32 offset, quint16 value)
{
	FT_STATUS d2xx_status;
	D2XX_CALL(FT_WriteEE, d->handle, offset, value);
	return FT_OK == d2xx_status;
}

bool QtFtdiChip::eraseEE()
{
	FT_STATUS d2xx_status;
	D2XX_CALL(FT_EraseEE, d->handle);
	return FT_OK == FT_EraseEE(d->handle);
}

bool QtFtdiChip::FTDI_EEPROM_Program(const char* description, const char* serial)
{
	QMutexLocker locker(&d->mutex_handle);
	FT_PROGRAM_DATA eeprom;

	char ManufacturerBuf[32];
	char ManufacturerIdBuf[16];
	char DescriptionBuf[64];
	char SerialNumberBuf[16];

	eeprom.Signature1 = 0x00000000;
	eeprom.Signature2 = 0xffffffff;
	eeprom.Version    = 0x00000004;
	eeprom.Manufacturer   = ManufacturerBuf;
	eeprom.ManufacturerId = ManufacturerIdBuf;
	eeprom.Description    = DescriptionBuf;
	eeprom.SerialNumber   = SerialNumberBuf;

	FT_STATUS d2xx_status;

	D2XX_CALL(FT_EE_Read, d->handle, &eeprom);
	if( d2xx_status != FT_OK ){
		setErrorString(tr("%1: failed (%2) to read current EEPROM setup!").arg(Q_FUNC_INFO).arg(d2xx_status));
		return false;
	}

	if( description ){
		strncpy(DescriptionBuf, description, 63);
		DescriptionBuf[63] = '\0';
	}

	if( serial ){
		strncpy(SerialNumberBuf, serial, 15);
		SerialNumberBuf[15] = '\0';
	}
	
	D2XX_CALL(FT_EE_Program, d->handle, &eeprom);
	if( d2xx_status != FT_OK ){
		setErrorString( tr("%1: failed (%2) to programm new EEPROM setup!").arg(Q_FUNC_INFO).arg(d2xx_status));
		return false;
	}

	return true;
}

#undef D2XX_CALL_RETURN_BOOLEAN
#undef D2XX_CALL_RETURN_BOOLEAN0

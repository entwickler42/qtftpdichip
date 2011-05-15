#include "qtftdichip_p.h"
#include "qtftdichipworker_libftdi.h"

#include <QTime>
#include <QCoreApplication>


void QtFtdiChipPrivate::createIOThreads()
{
	writer_thread = new QtFtdiChipWriter(this);
	reader_thread = new QtFtdiChipReader(this);
	connect(writer_thread, SIGNAL(bytesWritten(qint64)), this, SLOT(bytesWritten(qint64)));
	connect(reader_thread, SIGNAL(readyRead()), this, SLOT(readyRead()));
	writer_thread->start(QThread::NormalPriority);
	reader_thread->start(QThread::NormalPriority);
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
	if( isOpen() )
		return true;

	ftdi_init(&d->handle);
	ftdi_set_bitmode(&d->handle, 0xFF, BITMODE_BITBANG);
	int status = 0;
	if( d->open_by_index ){
		status = ftdi_usb_open_desc_index(&d->handle, 0x0403, 0xe8d8, NULL, NULL, d->index);
	}else{
		status = ftdi_usb_open_desc(&d->handle, 0x0403, 0xe8d8, NULL, qPrintable(d->serial));
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
	int status = ftdi_usb_close(&d->handle);
	d->mutex_handle.unlock();

	if( status == 0 ){
		QIODevice::close();
	}else{
		setErrorString(tr("%1: failed (%2) to close the device! ").arg(Q_FUNC_INFO).arg(status));
		d->createIOThreads();
	}

	ftdi_usb_close(&d->handle);
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
	return true;
}

bool QtFtdiChip::resetPort()
{
	qWarning("%s: not supported by ftdi backend - calling resetDevice() instead", Q_FUNC_INFO);
	return resetDevice();
}

bool QtFtdiChip::resetDevice()
{
	int status = ftdi_usb_reset(&d->handle);
	return status == 0; // -1 FTDI reset failed -2 USB Device unavailable
}

bool QtFtdiChip::purge(bool tx, bool rx)
{
	QMutexLocker lock_handle(&d->mutex_handle);

	int status = 0;

	if( tx )
		status += ftdi_usb_purge_tx_buffer(&d->handle);
	if( rx )
		status += ftdi_usb_purge_rx_buffer(&d->handle);

	return status == 0;
}

bool QtFtdiChip::setTimeouts(quint32 readTimeout, quint32 writeTimeout)
{
	qWarning("%s: not supported by ftdi backend", Q_FUNC_INFO);
	return true;
}

bool QtFtdiChip::setResetPipeRetryCount(quint32 count)
{
	qWarning("%s: not supported by ftdi backend", Q_FUNC_INFO);
	return true;
}

bool QtFtdiChip::setFlowControl(quint16 flowControl, uchar xon, uchar xoff)
{
	int status = ftdi_setflowctrl(&d->handle, SIO_DISABLE_FLOW_CTRL);
	return status == 0;
}

bool QtFtdiChip::setDataCharacteristics(uchar wordlength, uchar stopbits, uchar parity)
{
	int status = ftdi_set_line_property(&d->handle, BITS_8, STOP_BIT_1, NONE);
	return status == 0;
}

bool QtFtdiChip::setBaudRate(quint32 bautrate)
{
	int status = ftdi_set_baudrate(&d->handle, bautrate);
	return status == 0;
}

bool QtFtdiChip::setChars(uchar eventCh, uchar eventChEn, uchar errorCh, uchar errorChEn)
{
	int status = 0;
	status += ftdi_set_event_char(&d->handle, eventCh, eventChEn);
	status += ftdi_set_error_char(&d->handle, errorCh, errorChEn);
	return status == 0;
}

bool QtFtdiChip::setUSBParameters(quint32 inTransferSize, quint32 outTransferSize)
{
	qWarning("%s: not supported by ftdi backend", Q_FUNC_INFO);
	return true;
}

bool QtFtdiChip::setLatencyTime(uchar latency)
{
	int status = ftdi_set_latency_timer(&d->handle, latency);
	return status == 0;
}

bool QtFtdiChip::getDeviceInfo(QtFtdiChipInformation* info)
{
	QMutexLocker locker(&d->mutex_handle);


	info->m_id = d->index;
	info->m_serial = tr("-ERROR-");
	info->m_description = tr("-ERROR-");

	unsigned char* eeprom = new unsigned char[d->handle.eeprom_size];

	int status = ftdi_read_eeprom(&d->handle, eeprom);

	if( status == 0 ){
		struct ftdi_eeprom s_eeprom;
		status = ftdi_eeprom_decode(&s_eeprom, eeprom, d->handle.eeprom_size);

		if( status == 0 ){
			info->m_id = d->index;
			info->m_serial = QString::fromAscii(s_eeprom.serial);
			info->m_description = QString::fromAscii(s_eeprom.product);
		}
	}

	delete eeprom;
	return status == 0;
}

bool QtFtdiChip::readEE(quint32 offset, quint16* value)
{
	return false;
}

bool QtFtdiChip::writeEE(quint32 offset, quint16 value)
{
	return false;
}

bool QtFtdiChip::eraseEE()
{
	return false;
}

bool QtFtdiChip::FTDI_EEPROM_Program(const char* description, const char* serial)
{
	QMutexLocker locker(&d->mutex_handle);
	return false;
}


#ifndef QTFTDICHIP_H
#define QTFTDICHIP_H

#include "qtftdichip_global.h"
#include "qtftdichiplist.h"

#include <QIODevice>

class QtFtdiChipPrivate;
class QWaitCondition;
class QMutex;

class QTFTDICHIPSHARED_EXPORT QtFtdiChip :
		public QIODevice
{
    Q_OBJECT

	friend class QtFtdiChipPrivate;

public:
	QtFtdiChip(const QString& serial, QObject *parent = 0);
	QtFtdiChip(int index, QObject *parent = 0);

	virtual ~QtFtdiChip();

	bool isSequential() const;

	bool open(OpenMode mode);
	void close();

	qint64 bytesAvailable() const;
	qint64 bytesToWrite() const;

	bool waitForReadyRead(int msecs);
	bool waitForBytesWritten(int msecs);

	static bool addVidPid(quint32 vid, quint32 pid);

	bool resetPort();
	bool resetDevice();
	bool purge(bool tx = true, bool rx = true);
	bool setTimeouts(quint32 readTimeout, quint32 writeTimeout);
	bool setResetPipeRetryCount(quint32 count);
	bool setFlowControl(quint16 flowControl, uchar xon, uchar xoff);
	bool setDataCharacteristics(uchar wordlength, uchar stopbits, uchar parity);
	bool setBaudRate(quint32 bautrate);
	bool setChars(uchar eventCh, uchar eventChEn, uchar errorCh, uchar errorChEn);
	bool setUSBParameters(quint32 inTransferSize, quint32 outTransferSize);
	bool setLatencyTime(uchar latency);
	bool getDeviceInfo(QtFtdiChipInformation* info);

	bool setDescription(const char* description)
	{ return FTDI_EEPROM_Program(description, 0); }

	bool setSerial(const char* serial)
	{ return FTDI_EEPROM_Program(0, serial); }

	bool readEE(quint32 offset, quint16* value);
	bool writeEE(quint32 offset, quint16 value);
	bool eraseEE();

protected:
	qint64 readData(char *data, qint64 maxlen);
	qint64 writeData(const char *data, qint64 len);

private:
	QtFtdiChipPrivate* d;
	
	bool FTDI_EEPROM_Program(const char* description, const char* serial);
};

#endif

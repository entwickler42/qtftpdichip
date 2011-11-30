#ifndef QTFTDICHIPLISTWORKER_D2XX_H
#define QTFTDICHIPLISTWORKER_D2XX_H

#include "qtftdichiplist.h"

#include <QThread>
#include <QScopedArrayPointer>

#ifdef Q_OS_WIN32
#include <windows.h>
#else
#include <WinTypes.h>
#endif

#include <ftd2xx.h>


class QtFtdiChipListWorker :
		public QThread
{
	Q_OBJECT

signals:
	void error(int);
	void entry(QtFtdiChipInformation);

public:
	QtFtdiChipListWorker(QObject* parent=0) :
		QThread(parent) 
	{
		setTerminationEnabled(true);
	}

	virtual ~QtFtdiChipListWorker()
	{
		if( isRunning() && !wait(1000) ){
			qDebug("%s: Forcing Thread Termination - expect unpredictable results!", Q_FUNC_INFO);
			terminate();
		}
	}

protected:
	void run()
	{
		DWORD c;
		QtFtdiChipInformation dev_info;
		
		if( FT_STATUS s = FT_CreateDeviceInfoList(&c) != FT_OK ){
			emit error(s);
			return;
		}

		QScopedArrayPointer<FT_DEVICE_LIST_INFO_NODE> in(new FT_DEVICE_LIST_INFO_NODE[c]);

		if( FT_STATUS s = FT_GetDeviceInfoList(in.data(), &c) != FT_OK ){
			emit error(s);
			return;
		}

		for(quint32 i=0; i<c; i++){
			DWORD dev_nr = (DWORD)i;
			char SerialNumber[16];
			char Description[64];

			if( FT_STATUS s = FT_ListDevices((PVOID)dev_nr, &SerialNumber, FT_LIST_BY_INDEX|FT_OPEN_BY_SERIAL_NUMBER) != FT_OK ){
				emit error(s);
				continue;
			}
			if( FT_STATUS s = FT_ListDevices((PVOID)dev_nr, &Description, FT_LIST_BY_INDEX|FT_OPEN_BY_DESCRIPTION) != FT_OK ){
				emit error(s);
				continue;
			}

			dev_info.m_id = i;
			dev_info.m_serial = QString::fromAscii(SerialNumber);
			dev_info.m_description = QString::fromAscii(Description);

			emit entry(dev_info);
		}
	}
};

#endif 

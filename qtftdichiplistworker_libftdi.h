#ifndef QTFTDICHIPLISTWORKER_LIBFTDI_H
#define QTFTDICHIPLISTWORKER_LIBFTDI_H

#include "qtftdichiplist.h"

#include <ftdi.h>
#include <QThread>

//
class QtFtdiChipListWorker :
		public QThread
{
	Q_OBJECT

signals:
	void error(int);
	void entry(QtFtdiChipInformation);

public:
	QtFtdiChipListWorker(QObject* parent=0) :
		QThread(parent) // This is going to be debugging-fun when destruction happens in the event loop and something goes wrong
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
		int index = 0;
		int status = 0;
		struct ftdi_context handle;
		struct ftdi_device_list *devlist;
		QtFtdiChipInformation dev_info;

		if( (status = ftdi_init(&handle)) < 0 ){
			emit error(status);
			return;
		}

		if( (status = ftdi_usb_find_all(&handle, &devlist, 0x0403, 0xe8d8)) < 0 ){
			emit error(status);
			return;
		}

		for( struct ftdi_device_list* cur=devlist; cur!= NULL; index++ ){
			char manufacturer[128], description[128], serial[64];
			if( (status = ftdi_usb_get_strings(&handle, cur->dev, manufacturer, 128, description, 128, serial, 64)) < 0 ){
				emit error(status);
			}else{
				dev_info.m_id = index;
				dev_info.m_serial = QString::fromAscii(serial);
				dev_info.m_description = QString::fromAscii(description);
				emit entry(dev_info);
			}
			cur = cur->next;
		}

		ftdi_list_free(&devlist);
		ftdi_deinit(&handle);
	}
};

#endif 

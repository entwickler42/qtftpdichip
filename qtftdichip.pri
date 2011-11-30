isEmpty(QTFTDICHIP_BACKEND):QTFTDICHIP_BACKEND=$$(QTFTDICHIP_BACKEND)
isEmpty(QTFTDICHIP_BACKEND){
	error("QTFTDICHIP_BACKEND NOT DEFINED!")
}

exists($$QTFTDICHIP_BACKEND/src/ftdi.h){	
	LIBFTDI=$$QTFTDICHIP_BACKEND
}else:exists($$QTFTDICHIP_BACKEND/ftd2xx.h){	
	D2XX=$$QTFTDICHIP_BACKEND
}else{
	error("QTFTDICHIP_BACKEND INVALID!")	
}

!isEmpty(LIBFTDI){
	message("QtFtdiChip backend: libftdi (LGPL 2.1)")
	CONFIG += libftdi
	DEFINES += QTFTDICHIP_LIBFTDI
	INCLUDEPATH += $$LIBFTDI/src
	HEADERS += $$LIBFTDI/src/ftdi.h
	SOURCES += $$LIBFTDI/src/ftdi.c	
}else:!isEmpty(D2XX){	
	CONFIG += d2xx
	DEFINES += QTFTDICHIP_D2XX
	INCLUDEPATH += $$D2XX
	HEADERS += $$D2XX/ftd2xx.h
	win32{
		contains(QMAKE_HOST.arch, x86_64){
			message("QtFtdiChip backend: win32 x86_64/d2xx (commercial)")
			LIBS += -L$$D2XX/amd64/ -lftd2xx			
			
			!isEmpty(DESTDIR){
				QTFTDICHIP_COPY_DLL.commands = copy $$D2XX/x86_64/ftd2xx.dll $$DESTDIR/ftd2xx.dll
			}else:CONFIG(release, debug|release){
				QTFTDICHIP_COPY_DLL.commands = copy $$D2XX/x86_64/ftd2xx.dll $$OUT_PWD/release/ftd2xx.dll
			}else{
				QTFTDICHIP_COPY_DLL.commands = copy $$D2XX/x86_64/ftd2xx.dll $$OUT_PWD/debug/ftd2xx.dll
			}
		}else{
			message("QtFtdiChip backend: win32 i386/d2xx ( commercial )")			
			LIBS += -L$$D2XX/i386/ -lftd2xx
			!isEmpty(DESTDIR){
				QTFTDICHIP_COPY_DLL.commands = copy $$D2XX/i386/ftd2xx.dll $$DESTDIR/ftd2xx.dll
			}else:CONFIG(release, debug|release){
				QTFTDICHIP_COPY_DLL.commands = copy $$D2XX/i386/ftd2xx.dll $$OUT_PWD/release/ftd2xx.dll
			}else{
				QTFTDICHIP_COPY_DLL.commands = copy $$D2XX/i386/ftd2xx.dll $$OUT_PWD/debug/ftd2xx.dll
			}
		}
		QTFTDICHIP_COPY_DLL.target = ftd2xx.dll	
		QTFTDICHIP_COPY_DLL.commands = $$replace(QTFTDICHIP_COPY_DLL.commands, /, \\)
		QMAKE_EXTRA_TARGETS += QTFTDICHIP_COPY_DLL
		PRE_TARGETDEPS += ftd2xx.dll		
	}else:unix{				
		HEADERS += $$D2XX/ftd2xx.h $$D2XX/WinTypes.h
		contains(QMAKE_HOST.arch, x86_64){
			message("QtFtdiChip backend: unix x86_64/d2xx (commercial)")
			LIBS += $$D2XX/build/x86_64/libftd2xx.a
		}else{
			message("QtFtdiChip backend: unix i386/d2xx (commercial)")
			LIBS += $$D2XX/build/i386/libftd2xx.a
		}
	}else{
		error("PLATFORM NOT SUPPORTED!")
	}
}else{
	error("NO FTDI CHIP BACKEND FOUND!")
}

INCLUDEPATH += $$PWD
DEFINES += QTFTDICHIP_LIBRARY

SOURCES += $$PWD/qtftdichiplist.cpp

HEADERS += $$PWD/qtftdichip_global.h \
	$$PWD/qtftdichip.h \
	$$PWD/qtftdichip_p.h \
	$$PWD/qtftdichiplist.h \
	$$PWD/qtftdichiplist_p.h \
	$$PWD/qtftdichiplist.h
libftdi{
	SOURCES += $$PWD/qtftdichip_libftdi.cpp
	HEADERS += $$PWD/qtftdichipworker_libftdi.h \
		$$PWD/qtftdichiplistworker_libftdi.h
}else:d2xx{
	SOURCES += $$PWD/qtftdichip_d2xx.cpp 
	HEADERS += $$PWD/qtftdichipworker_d2xx.h \
		$$PWD/qtftdichiplistworker_d2xx.h 
}else{
	error("BUG: WE SHOULD HAVE NEVER GOTTEN HERE!")
}

message($$SOURCES)
message($$HEADERS)
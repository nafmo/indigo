#DEBUG=-DDEBUG

indigo.exe: indigo.cpp areas.h pkthead.h bluewave.h debug.h config.h areas.obj config.obj fido.obj uplink.obj version.h shorttag.obj crc32.obj msgid.obj
        tcc -mc $(DEBUG) indigo.cpp areas.obj fido.obj config.obj uplink.obj shorttag.obj crc32.obj msgid.obj spawnc.lib

fido.obj: fido.cpp areas.h fido.h debug.h
        tcc -mc -c $(DEBUG) fido.cpp

areas.obj: areas.cpp areas.h fido.h debug.h shorttag.h
        tcc -mc -c $(DEBUG) areas.cpp

config.obj: config.cpp config.h fido.h debug.h uplink.h areas.h
        tcc -mc -c $(DEBUG) config.cpp

uplink.obj: uplink.cpp uplink.h fido.h debug.h version.h shorttag.h msgid.h
        tcc -mc -c $(DEBUG) uplink.cpp

shorttag.obj: shorttag.cpp shorttag.h crc32.h datatyp.h
        tcc -mc -c $(DEBUG) shorttag.cpp

crc32.obj: crc32.cpp crc32.h
        tcc -mc -c $(DEBUG) crc32.cpp

msgid.obj: msgid.cpp msgid.h crc32.h datatyp.h
        tcc -mc -c $(DEBUG) msgid.cpp

clean:
        del *.obj *.bak

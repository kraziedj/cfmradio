CFLAGS?=-Os -g -Wall
LDFLAGS?=-Wl,--as-needed

MISC_CFLAGS:=-std=gnu99 -DG_LOG_DOMAIN=\"CFmRadio\"
PKGCONFIG_PKGS:=libosso hildon-1 alsa libpulse-mainloop-glib dbus-glib-1 gconf-2.0
PKGCONFIG_CFLAGS:=$(shell pkg-config $(PKGCONFIG_PKGS) --cflags)
PKGCONFIG_LIBS:=$(shell pkg-config $(PKGCONFIG_PKGS) --libs)
LAUNCHER_CFLAGS:=$(shell pkg-config maemo-launcher-app --cflags)
LAUNCHER_LDFLAGS:=$(shell pkg-config maemo-launcher-app --libs)

OBJS=cfmradio.o radio.o types.o tuner.o rds.o presets.o preset_list.o

all: cfmradio.launch

cfmradio.launch: $(OBJS)
	$(CC) $(LAUNCHER_LDFLAGS) $(LDFLAGS) -o $@ $^ $(PKGCONFIG_LIBS) $(LIBS)

cfmradio: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(PKGCONFIG_LIBS) $(LIBS)

$(OBJS): %.o: %.c
	$(CC) $(MISC_CFLAGS) $(PKGCONFIG_CFLAGS) $(LAUNCHER_CFLAGS) $(CFLAGS) -o $@ -c $<

radio.c: radio.h types.h n900-fmrx-enabler.h

n900-fmrx-enabler.h: n900-fmrx-enabler.xml
	dbus-binding-tool --mode=glib-client --output=$@ --prefix=fmrx_enabler $<

install: cfmradio.launch
	install -m 0755 $(IFLAGS) cfmradio.launch $(DESTDIR)/usr/bin/
	ln -sf /usr/bin/maemo-invoker $(DESTDIR)/usr/bin/cfmradio
	install -m 0644 $(IFLAGS) data/icon.png \
		$(DESTDIR)/usr/share/icons/hicolor/scalable/hildon/cfmradio.png
	install -m 0644 $(IFLAGS) data/cfmradio.service \
		$(DESTDIR)/usr/share/dbus-1/services/
	install -m 0644 $(IFLAGS) data/cfmradio.desktop \
		$(DESTDIR)/usr/share/applications/hildon/

clean:
	rm -f cfmradio cfmradio.launch *.o

.PHONY: all clean


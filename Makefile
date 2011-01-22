CFLAGS?=-O0 -g -Wall
LDFLAGS?=-Wl,--as-needed

LOCALEDIR?=/usr/share/locale

GETTEXT_PACKAGE:=cfmradio
GETTEXT_CFLAGS:=-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)\" -DLOCALEDIR=\"$(LOCALEDIR)\"
PKGCONFIG_PKGS:=libosso hildon-1 alsa libpulse-mainloop-glib dbus-glib-1 gconf-2.0
PKGCONFIG_CFLAGS:=$(shell pkg-config $(PKGCONFIG_PKGS) --cflags)
PKGCONFIG_LIBS:=$(shell pkg-config $(PKGCONFIG_PKGS) --libs)
LAUNCHER_CFLAGS:=$(shell pkg-config maemo-launcher-app --cflags) -fvisibility=hidden
LAUNCHER_LDFLAGS:=$(shell pkg-config maemo-launcher-app --libs)
MISC_CFLAGS:=-std=gnu99 -DG_LOG_DOMAIN=\"CFmRadio\"

SRCS:=cfmradio.c radio.c radio_routing.c types.c tuner.c rds.c \
	presets.c preset_list.c preset_renderer.c
OBJS:=$(SRCS:.c=.o)
POT:=po/$(GETTEXT_PACKAGE).pot
PO_FILES:=$(wildcard po/*.po)
MO_FILES:=$(PO_FILES:.po=.mo)
LANGS:=$(basename $(notdir $(PO_FILES)))

all: cfmradio.launch $(MO_FILES)

cfmradio.launch: $(OBJS)
	$(CC) $(LAUNCHER_LDFLAGS) $(LDFLAGS) -o $@ $^ $(PKGCONFIG_LIBS) $(LIBS)

cfmradio: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(PKGCONFIG_LIBS) $(LIBS)

$(OBJS): %.o: %.c
	$(CC) $(GETTEXT_CFLAGS) $(MISC_CFLAGS) $(PKGCONFIG_CFLAGS) $(LAUNCHER_CFLAGS) $(CFLAGS) -o $@ -c $<

radio.c: radio.h types.h n900-fmrx-enabler.h

n900-fmrx-enabler.h: n900-fmrx-enabler.xml
	dbus-binding-tool --mode=glib-client --output=$@ --prefix=fmrx_enabler $<

$(POT): $(SRCS)
	xgettext --default-domain=$(GETTEXT_PACKAGE) --from-code=UTF-8 --language=C \
	--msgid-bugs-address=maemo@javispedro.com --package-name=CFmRadio \
	--keyword=_ -o$@ $^

$(MO_FILES): %.mo: %.po
	msgfmt -c -o $@ $<

$(PO_FILES): %: $(POT)
	msgmerge -U $@ $(POT)
	@touch $@

install: cfmradio.launch
	install -m 0755 $(IFLAGS) cfmradio.launch $(DESTDIR)/usr/bin/
	ln -sf /usr/bin/maemo-invoker $(DESTDIR)/usr/bin/cfmradio
	install -m 0644 $(IFLAGS) data/icon.png \
		$(DESTDIR)/usr/share/icons/hicolor/scalable/hildon/cfmradio.png
	install -m 0644 $(IFLAGS) data/cfmradio.service \
		$(DESTDIR)/usr/share/dbus-1/services/
	install -m 0644 $(IFLAGS) data/cfmradio.desktop \
		$(DESTDIR)/usr/share/applications/hildon/
	for lang in $(LANGS); do \
		install -d $(DESTDIR)$(LOCALEDIR)/$$lang/LC_MESSAGES ; \
		install -m 0644 po/$$lang.mo \
			$(DESTDIR)$(LOCALEDIR)/$$lang/LC_MESSAGES/$(GETTEXT_PACKAGE).mo ; \
	done

clean:
	rm -f cfmradio cfmradio.launch *.o $(MO_FILES)

.PHONY: all clean


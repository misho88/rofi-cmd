CFLAGS=-shared -fPIC -DPIC -Wall $(shell pkg-config --cflags glib-2.0 cairo)
LDFLAGS=$(shell pkg-config --libs glib-2.0 gobject-2.0 gio-2.0 gmodule-2.0 cairo)

all: cmd.so

cmd.so: cmd.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

clean:
	rm cmd.so

install: cmd.so
	install cmd.so /usr/lib/rofi
	install rofi-cmd /usr/local/bin

uninstall:
	rm /usr/lib/rofi/cmd.so || true
	rm /usr/local/bin/rofi-cmd || true

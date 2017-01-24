CFLAGS = -g -Wall -Wextra -Wno-unused-parameter `pkg-config --cflags webkit2gtk-4.0 cairo-xlib x11`
LDFLAGS = `pkg-config --libs webkit2gtk-4.0 cairo-xlib x11`

all: wrapper.c
	gcc ${CFLAGS} -o webkit-wrapper wrapper.c ${LDFLAGS}

clean:
	rm *~ *.o webkit-wrapper

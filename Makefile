PREFIX ?= /usr/local
PRG := gwfmt

# Warnings
CFLAGS += -Wall -Wextra -Wno-unused

# Includes
CFLAGS += -I../util/include
CFLAGS += -I../ast/include
CFLAGS += -I../util/libtermcolor/include
CFLAGS += -Iinclude

# Libraries
LDFLAGS += -L../util
LDFLAGS += -L../ast
LDFLAGS += -L../util/libtermcolor
LDFLAGS += -L../ast/libprettyerr

#CFLAGS += -flto -Ofast
#LDFLAGS += -flto

LDFLAGS += -lprettyerr -ltermcolor

ifneq ($(shell uname), Darwin)
LDFLAGS += -static
endif

ifeq ($(shell uname), Darwin)
AR = /usr/bin/libtool
AR_OPT = -static $^ -o $@
LDFLAGS += -undefined dynamic_lookup
else
AR = ar
AR_OPT = rcs $@ $^
endif

ifeq (${BUILD_ON_WINDOWS}, 1)
CFLAGS += -DBUILD_ON_WINDOWS=1 -D_XOPEN_SOURCE=700
LDFLAGS += -Wl,--enable-auto-import
endif

all: ${PRG}

${PRG}: src/${PRG}.o src/unpy.o libgwion-fmt.a
	${CC} ${CFLAGS} $? -Iinclude -lgwion_ast -lgwion_util ${LDFLAGS} -lpthread -lm -o ${PRG}

libgwion-fmt.a: src/lint.o
	${AR} ${AR_OPT}

src/unpy.c: src/unpy.l
	${LEX} src/unpy.l

clean:
	rm -rf src/*.o ${PRG} libgwion-fmt.a

install: all
	install ${PRG} ${PREFIX}/bin
	install lib${PRG}.a ${PREFIX}/lib
	install include/${PRG}.h ${PREFIX}/include/${PRG}.h

uninstall:
	rm ${PREFIX}/bin/${PRG}
	rm ${PREFIX}/lib/lib${PRG}.a
	rm ${PREFIX}/include/${PRG}.h

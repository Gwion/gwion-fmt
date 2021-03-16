PREFIX ?= /usr/local
PRG := gwion-fmt

# Warnings
CFLAGS += -Wall -Wextra -Wno-unused

# Includes
CFLAGS += -I/usr/local/include/gwion/util
CFLAGS += -I/usr/local/include/gwion/ast
CFLAGS += -I/usr/local/include/libtermcolor
CFLAGS += -Iinclude

#CFLAGS += -flto -Ofast
#LDFLAGS += -flto

LDFLAGS += -static -lprettyerr -ltermcolor

all: src/lint.c src/unpy.c
	${CC} ${CFLAGS} $? -Iinclude -lgwion_ast -lgwion_util ${LDFLAGS} -lpthread -lm -o ${PRG}

src/unpy.c:
	${LEX} src/unpy.l

clean:
	rm -rf src/*.o ${PRG}

install: all
	install ${PRG} ${PREFIX}/bin

uninstall:
	rm ${PREFIX}/bin/${PRG}

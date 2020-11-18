PREFIX ?= /usr/local
PRG := gwion-fmt

# Warnings
CFLAGS += -Wall -Wextra -Wno-unused

# Includes
CFLAGS += -I/usr/local/include/gwion/util
CFLAGS += -I/usr/local/include/gwion/ast
CFLAGS += -Iinclude

CFLAGS += -flto -Ofast
LDFLAGS += -flto

all: src/lint.c src/unpy.c
	${CC} ${CFLAGS} ${LDFLAGS} $? -Iinclude -lpthread -lgwion_ast -lgwion_util -lm -o ${PRG}

src/unpy.c:
	${LEX} src/unpy.l

clean:
	rm -rf src/*.o ${PRG}

install: all
	install ${PRG} ${PREFIX}/bin

uninstall:
	rm ${PREFIX}/bin/${PRG}

# Warnings
CFLAGS += -Wall -Wextra -Wno-unused

# Includes
CFLAGS += -I/usr/local/include/gwion/util
CFLAGS += -I/usr/local/include/gwion/ast
CFLAGS += -Iinclude

CFLAGS += -flto -Ofast
LDFLAGS += -flto

all: src/lint.c src/unpy.c
#	${CC} ${CFLAGS} ${LDFLAGS} $? -I ~/src/git/Gwion/include -I/home/djay/src/git/Gwion/util/include -I/home/djay/src/git/Gwion/ast/include -lpthread -lgwion_ast -lgwion_util -lm -o gwion-lint
	${CC} ${CFLAGS} ${LDFLAGS} $? -I/usr/local/include/gwion/util -Iinclude -lpthread -lgwion_ast -lgwion_util -lm -o gwion-lint

src/unpy.c:
	${LEX} src/unpy.l

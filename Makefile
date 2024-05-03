PREFIX ?= /usr/local
PRG := gwfmt

CFLAGS += -O3

# Warnings
CFLAGS += -Wall -Wextra

# Includes
CFLAGS += -I../util/include
CFLAGS += -I../ast/include
CFLAGS += -I../include
CFLAGS += -Iinclude

# Libraries
LDFLAGS += -L../util
LDFLAGS += -L../ast
LDFLAGS += -L../

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

${PRG}: src/${PRG}.o src/unpy.o src/gwion_config.o libgwion_fmt.a
	${CC} ${DEPFLAGS} ${CFLAGS} $^ -Iinclude -lgwion -lgwion_ast -lgwion_util ${LDFLAGS} -ldl -lpthread -lm -o $@

libgwion_fmt.a: src/lint.o src/casing.o
	${AR} ${AR_OPT}

src/unpy.c: src/unpy.l
	${LEX} src/unpy.l

clean:
	rm -rf src/*.o ${PRG} libgwion_fmt.a

install: all
	install ${PRG} ${PREFIX}/bin
	install lib${PRG}.a ${PREFIX}/lib
	install include/${PRG}.h ${PREFIX}/include/${PRG}.h

uninstall:
	rm ${PREFIX}/bin/${PRG}
	rm ${PREFIX}/lib/lib${PRG}.a
	rm ${PREFIX}/include/${PRG}.h

.c.o:
	$(info compile $(<:.c=))
	@${CC} $(DEPFLAGS) ${CFLAGS} ${PACKAGE_INFO} ${INSTALL_PREFIX} -c $< -o $(<:.c=.o)
	@mv -f $(DEPDIR)/$(@F:.o=.Td) $(DEPDIR)/$(@F:.o=.d) && touch $@
	@echo $@: config.mk >> $(DEPDIR)/$(@F:.o=.d)

DEPDIR := .d
$(shell mkdir -p $(DEPDIR) >/dev/null)
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$(@F:.o=.Td)

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

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)

all: ${PRG}

${PRG}: build/${PRG}.o build/unpy.o build/gwion_config.o libgwion_fmt.a
	${CC} ${DEPFLAGS} ${CFLAGS} $^ -Iinclude -lgwion -lgwion_ast -lgwion_util ${LDFLAGS} -ldl -lpthread -lm -o $@

libgwion_fmt.a: build/lint.o build/casing.o
	${AR} ${AR_OPT}

src/unpy.c: src/unpy.l
	${LEX} src/unpy.l

clean:
	rm -rf build/*.o ${PRG} libgwion_fmt.a

install: all
	install ${PRG} ${PREFIX}/bin
	install lib${PRG}.a ${PREFIX}/lib
	install include/${PRG}.h ${PREFIX}/include/${PRG}.h

uninstall:
	rm ${PREFIX}/bin/${PRG}
	rm ${PREFIX}/lib/lib${PRG}.a
	rm ${PREFIX}/include/${PRG}.h

build/%.o: $(subst build,src, $(@:.o=.c))
	$(info compile $(subst build/,,$(@:.o=)))
	@mkdir -p $(shell dirname $@) > /dev/null
	@mkdir -p $(subst build,.d,$(shell dirname $@)) > /dev/null
	@${CC} $(DEPFLAGS) ${CFLAGS} -c $(subst build,src,$(@:.o=.c)) -o $@
	@mv -f $(subst build,${DEPDIR},$(@:.o=.Td)) $(subst build,${DEPDIR},$(@:.o=.d)) && touch $@

DEPDIR := .d
$(shell mkdir -p $(DEPDIR) >/dev/null)
DEPFLAGS = -MT $@ -MMD -MP -MF $(subst build,${DEPDIR},$(@:.o=.Td))
DEPS := $(subst build,$(DEPDIR),$(OBJ:.o=.d))
-include $(DEPS)

ifeq ($(OS),Windows_NT)
  EXE := .exe
else
  EXE :=
endif

BIN1=gunpak
BIN2=gunlxr
BIN3=gung1t
SRC1=gunpak.c util.c
SRC2=gunlxr.c util.c miniz_tinfl.c
SRC3=gung1t.c util.c
OBJ1=${SRC1:.c=.o}
DEP1=${SRC1:.c=.d}
OBJ2=${SRC2:.c=.o}
DEP2=${SRC2:.c=.d}
OBJ3=${SRC3:.c=.o}
DEP3=${SRC3:.c=.d}

BIN=${BIN1}${EXE} ${BIN2}${EXE} ${BIN3}${EXE}
OBJ=${OBJ1} ${OBJ2} ${OBJ3}

CFLAGS=-std=c99 -pipe -fvisibility=hidden -Wall -Wextra -Werror -DNDEBUG -D_GNU_SOURCE -O2
LDFLAGS=-s

.PHONY: all clean

all: ${BIN}

clean:
	@${RM} ${BIN} ${OBJ} ${DEP}

${BIN1}${EXE}: ${OBJ1}
	@echo [L] $@
	@${CC} ${LDFLAGS} -o $@ $^

${BIN2}${EXE}: ${OBJ2}
	@echo [L] $@
	@${CC} ${LDFLAGS} -o $@ $^

${BIN3}${EXE}: ${OBJ3}
	@echo [L] $@
	@${CC} ${LDFLAGS} -o $@ $^

%.o: %.c
	@echo [C] $<
	@${CC} ${CFLAGS} -MMD -c -o $@ $<

-include ${DEP}

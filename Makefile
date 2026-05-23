PROGRAM = roguec
CMOC ?= cmoc
LWASM ?= lwasm
CMOC_OS9 ?= ../coco-shelf/cmoc_os9
NITROS9 ?= ../coco-shelf/nitros9
CFLAGS = --os9 -O2 --function-stack=0 --add-os9-stack-space=256 -I. -I$(CMOC_OS9)/include
ASFLAGS = --obj -I$(CMOC_OS9)/include
LDLIBS = -L$(CMOC_OS9)/lib -lc
ROGUE_DAT_SRC ?= $(NITROS9)/3rdparty/packages/rogue/rogue.dat
ROGUE_HLP_SRC ?= $(NITROS9)/3rdparty/packages/rogue/rogue.hlp
ROGUE_CHR_SRC ?= $(NITROS9)/3rdparty/packages/rogue/rogue.chr
ROGUE_SCR_SRC ?= $(NITROS9)/3rdparty/packages/rogue/rogue.scr

SRCS = main.c epyx_arena.c epyx_format.c epyx_screen.c rogue_game.c
ASRCS = rogue_signal.as
OBJS = $(SRCS:.c=.o) $(ASRCS:.as=.o)
SMALL_OBJS = $(SRCS:.c=.small.o) $(ASRCS:.as=.small.o)
STATIC_OBJS = $(SRCS:.c=.static.o) $(ASRCS:.as=.static.o)

all: $(PROGRAM) roguec-small roguec-heap roguec-static rogue.dat rogue.hlp rogue.chr rogue.scr

small: roguec-small rogue.dat rogue.hlp rogue.chr rogue.scr

static: roguec-static rogue.dat rogue.hlp rogue.chr rogue.scr

heap: $(PROGRAM) roguec-heap rogue.dat rogue.hlp rogue.chr rogue.scr

$(PROGRAM): $(OBJS)
	$(CMOC) $(CFLAGS) -DROGUE_HEAP_ARENA=1 -o $@ $(OBJS) $(LDLIBS)

roguec-small: $(SMALL_OBJS)
	$(CMOC) $(CFLAGS) -DROGUE_HEAP_ARENA=1 -DROGUE_SMALL -o $@ $(SMALL_OBJS) $(LDLIBS)

roguec-heap: $(OBJS)
	$(CMOC) $(CFLAGS) -DROGUE_HEAP_ARENA=1 -o $@ $(OBJS) $(LDLIBS)

roguec-static: $(STATIC_OBJS)
	$(CMOC) $(CFLAGS) -o $@ $(STATIC_OBJS) $(LDLIBS)

rogue.dat: $(ROGUE_DAT_SRC)
	cp $< $@

rogue.hlp: $(ROGUE_HLP_SRC)
	cp $< $@

rogue.chr: $(ROGUE_CHR_SRC)
	cp $< $@

rogue.scr: $(ROGUE_SCR_SRC)
	cp $< $@

%.o: %.c
	$(CMOC) $(CFLAGS) -DROGUE_HEAP_ARENA=1 -c $<

%.o: %.as
	$(LWASM) $(ASFLAGS) -o$@ $<

%.small.o: %.c
	$(CMOC) $(CFLAGS) -DROGUE_HEAP_ARENA=1 -DROGUE_SMALL -o $@ -c $<

%.small.o: %.as
	$(LWASM) $(ASFLAGS) -o$@ $<

%.static.o: %.c
	$(CMOC) $(CFLAGS) -o $@ -c $<

%.static.o: %.as
	$(LWASM) $(ASFLAGS) -o$@ $<

clean:
	rm -f $(PROGRAM) roguec-small roguec-heap roguec-static rogue.dat rogue.hlp rogue.chr rogue.scr *.o *.s *.list *.map

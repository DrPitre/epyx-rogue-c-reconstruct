PROGRAM = roguec
CMOC ?= cmoc
LWASM ?= lwasm
NITROS9DIR ?= $(abspath ../coco-shelf/nitros9)
CMOC_OS9 ?= $(abspath $(NITROS9DIR)/../cmoc_os9)
CFLAGS = --os9 -O2 -fomit-frame-pointer --function-stack=0 --add-os9-stack-space=256 -I. -I$(CMOC_OS9)/include
ASFLAGS = --obj -I$(CMOC_OS9)/include
LDLIBS = -L$(CMOC_OS9)/lib -lc
ROGUE_DAT_SRC ?= $(NITROS9DIR)/3rdparty/packages/rogue/rogue.dat
ROGUE_HLP_SRC ?= $(NITROS9DIR)/3rdparty/packages/rogue/rogue.hlp
ROGUE_CHR_SRC ?= $(NITROS9DIR)/3rdparty/packages/rogue/rogue.chr
ROGUE_SCR_SRC ?= $(NITROS9DIR)/3rdparty/packages/rogue/rogue.scr
STARTUP ?= startup
LEVEL ?= 2
FLOPPY_DIR ?= $(NITROS9DIR)/recipes/coco3/floppy
# The full coco3 recipe disk has almost no free space left, so copying
# roguec and its data files onto it silently truncates them. The minimal
# recipe (shell + grfdrv only) leaves ~300K free, which is what we need.
FLOPPY_MINIMAL ?= 1
FLOPPY_RECIPE = $(if $(filter 1,$(FLOPPY_MINIMAL)),coco3_minimal,coco3)
FLOPPY_DSKIMAGE ?= l$(LEVEL)_$(FLOPPY_RECIPE).dsk
DSKIMAGE ?= roguec.dsk
ROGUE_DISK_DIR ?= ROGUE
OS9 ?= os9
OS9COPY = $(OS9) copy -o=0
OS9ATTR = $(OS9) attr -q
OS9ATTR_TEXT = $(OS9ATTR) -npe -npw -pr -ne -w -r
OS9ATTR_EXEC = $(OS9ATTR) -pe -npw -pr -e -w -r
OS9MAKDIR = $(OS9) makdir
MAME ?= mame
MAME_MACHINE ?= coco3
MAME_FLAGS ?= -window -nothrottle -skip_gameinfo -autoboot_delay 5 -autoboot_command "DOS\n" -ext fdc -ext:fdc:wd17xx:0 525qd
.DEFAULT_GOAL := all

SRCS = main.c epyx_arena.c epyx_format.c epyx_screen.c rogue_game.c
ASRCS = rogue_signal.as
OBJS = $(SRCS:.c=.o) $(ASRCS:.as=.o)
SMALL_OBJS = $(SRCS:.c=.small.o) $(ASRCS:.as=.small.o)
STATIC_OBJS = $(SRCS:.c=.static.o) $(ASRCS:.as=.static.o)
DISK_PROGRAMS = $(PROGRAM) roguec-small roguec-heap

all: $(PROGRAM) roguec-small roguec-heap roguec-static rogue.dat rogue.hlp rogue.chr rogue.scr

small: roguec-small rogue.dat rogue.hlp rogue.chr rogue.scr

static: roguec-static rogue.dat rogue.hlp rogue.chr rogue.scr

heap: $(PROGRAM) roguec-heap rogue.dat rogue.hlp rogue.chr rogue.scr

disk: $(DSKIMAGE)

run: disk
	$(MAME) $(MAME_MACHINE) $(MAME_FLAGS) -flop1 $(DSKIMAGE)

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

$(FLOPPY_DIR)/$(FLOPPY_DSKIMAGE):
	$(MAKE) -C $(FLOPPY_DIR) LEVEL=$(LEVEL) MINIMAL=$(FLOPPY_MINIMAL) STARTUP=$(abspath $(STARTUP))

$(DSKIMAGE): $(FLOPPY_DIR)/$(FLOPPY_DSKIMAGE) $(DISK_PROGRAMS) rogue.dat rogue.hlp rogue.chr rogue.scr $(STARTUP)
	cp $< $@
	$(OS9MAKDIR) $@,$(ROGUE_DISK_DIR)
	$(OS9COPY) rogue.dat rogue.hlp rogue.chr rogue.scr $@,$(ROGUE_DISK_DIR)
	$(OS9ATTR_TEXT) $(foreach f,rogue.dat rogue.hlp rogue.chr rogue.scr,$@,$(ROGUE_DISK_DIR)/$(f))
	$(OS9COPY) $(DISK_PROGRAMS) $@,CMDS
	$(OS9ATTR_EXEC) $(foreach f,$(DISK_PROGRAMS),$@,CMDS/$(f))

clean:
	rm -f $(PROGRAM) roguec-small roguec-heap roguec-static rogue.dat rogue.hlp rogue.chr rogue.scr *.o *.s *.list *.map $(DSKIMAGE)

.PHONY: all small static heap disk run clean

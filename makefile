# project name (generate executable with this name)
TARGET   = dwr512_phmanager

CROSS=mipsel-openwrt-linux
CC=${CROSS}-gcc
LD=${CROSS}-gcc

# compiling flags here
#CFLAGS   = -std=c99 -Wall -I.
CFLAGS   += -I./proslic

# linking flags here
#LFLAGS   = -Wall -I. -lm

# change these to proper directories where each file should be
SRCDIR   = src
OBJDIR   = obj
BINDIR   = bin

SOURCES  := $(wildcard $(SRCDIR)/*.c)
INCLUDES := $(wildcard $(SRCDIR)/*.h)
#OBJECTS  := $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
OBJECTS  := dwr_phmanager.o slic_ctrl.o modem_ctrl.o si3210_spi.o
OBJECTS  := $(addprefix $(OBJDIR)/,$(OBJECTS))
PSOBJECT := $(OBJDIR)/proslic.o
rm       = rm -f

$(BINDIR)/$(TARGET): $(OBJECTS) $(PSOBJECT) | $(BINDIR)
	$(LD) $(OBJECTS) $(PSOBJECT) $(LFLAGS) -o $@

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c | $(OBJDIR) proslic/proslic.c
	$(CC) $(CFLAGS) -c $< -o $@

$(PSOBJECT): proslic/proslic.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BINDIR):
	mkdir -p $(BINDIR)
$(OBJDIR):
	mkdir -p $(OBJDIR)

proslic/proslic.c :
	wget https://github.com/juergh/naspkg/archive/master.zip
	unzip -o master.zip naspkg-master/packages/kernel-modules/linux-2.6.12.6-arm1/arch/arm/mach-mv88fxx81/Board/slic/proslic.c -d proslic
	unzip -o master.zip naspkg-master/packages/kernel-modules/linux-2.6.12.6-arm1/arch/arm/mach-mv88fxx81/Board/slic/proslic.h -d proslic
	mv $$(find proslic/ -name proslic.c) proslic/
	mv $$(find proslic/ -name proslic.h) proslic/
	patch proslic/proslic.c proslic/proslic.patch
	rm -R proslic/naspkg-master
	rm master.zip

.PHONY: clean
clean:
	$(rm) -R $(OBJDIR)
	$(rm) proslic/proslic.c proslic/proslic.h
	@echo "Cleanup complete!"

.PHONY: remove
remove: clean
	@$(rm) -R $(BINDIR)
	@echo "Executable removed!"


# DOSAGENT — DOS Remote Agent
# OpenWatcom wmake Makefile — DOS 32-bit flat model, DOS/32A extender

WATCOM_DIR = $(%WATCOM)

CC = wcc386
LD = wlink
CFLAGS = -bt=dos -mf -3s -ox -zq -w4 -i=$(WATCOM_DIR)/h -i=scripts/lib/watt32/inc

SRCDIR = agent/src
OBJS = $(SRCDIR)/main.obj $(SRCDIR)/net.obj $(SRCDIR)/protocol.obj &
       $(SRCDIR)/exec.obj $(SRCDIR)/file.obj $(SRCDIR)/screen.obj &
       $(SRCDIR)/keys.obj

BINDIR = agent/bin
TARGET = $(BINDIR)/dosagent.exe

$(TARGET) : $(OBJS)
    $(LD) name $(TARGET) system dos32a option map=$(BINDIR)/dosagent.map option stack=16384 &
        option stub=$(WATCOM_DIR)/binw/dos32a.exe &
        library scripts/lib/watt32/lib/wattcpwf.lib &
        file {$(OBJS)}

{$(SRCDIR)}.c{$(SRCDIR)}.obj :
    $(CC) $(CFLAGS) -fo=$@ $[@

$(SRCDIR)/main.obj : $(SRCDIR)/main.c $(SRCDIR)/agent.h
$(SRCDIR)/net.obj : $(SRCDIR)/net.c $(SRCDIR)/agent.h
$(SRCDIR)/protocol.obj : $(SRCDIR)/protocol.c $(SRCDIR)/agent.h
$(SRCDIR)/exec.obj : $(SRCDIR)/exec.c $(SRCDIR)/agent.h
$(SRCDIR)/file.obj : $(SRCDIR)/file.c $(SRCDIR)/agent.h
$(SRCDIR)/screen.obj : $(SRCDIR)/screen.c $(SRCDIR)/agent.h
$(SRCDIR)/keys.obj : $(SRCDIR)/keys.c $(SRCDIR)/agent.h

clean : .SYMBOLIC
    rm -f $(SRCDIR)/*.obj $(SRCDIR)/*.err $(TARGET) $(BINDIR)/*.map *.err

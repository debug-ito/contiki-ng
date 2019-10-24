CONTIKI_PROJECT = mpmc-ring-interrupt
all: $(CONTIKI_PROJECT)

ifeq ($(TARGET),np108)
  MODULES += os/services/np108/simplelink-hw-debug
endif

MAKE_MAC=MAKE_MAC_NULLMAC
MAKE_NET=MAKE_NET_NULLNET

CONTIKI = ../../..
include $(CONTIKI)/Makefile.include

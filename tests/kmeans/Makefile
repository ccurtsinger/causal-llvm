ROOT = ../..
TARGETS = kmeans
LIBS = pthread dl
ARGS =

include $(ROOT)/common.mk

CFLAGS += -g
CXXFLAGS += -g

test:: kmeans
	$(PRELOAD_VAR)=$(ROOT)/libcausal.$(SHLIB_SUFFIX) ./kmeans $(ARGS)

ROOT = ../..
TARGETS = kmeans
LIBS = pthread
ARGS = 

include $(ROOT)/common.mk

test:: kmeans
	$(PRELOAD_VAR)=$(ROOT)/libcausal.$(SHLIB_SUFFIX) ./kmeans $(ARGS)

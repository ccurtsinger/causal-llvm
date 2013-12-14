ROOT = ../..
TARGETS = kmeans
LIBS = pthread dl
ARGS = 

include $(ROOT)/common.mk

test:: kmeans
	$(PRELOAD_VAR)=$(ROOT)/libcausal.$(SHLIB_SUFFIX) ./kmeans $(ARGS)

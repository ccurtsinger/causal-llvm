ROOT = ../..
TARGETS = foo
LIBS = pthread dl

include $(ROOT)/common.mk

test:: foo
	$(PRELOAD_VAR)=$(ROOT)/libcausal.$(SHLIB_SUFFIX) ./foo $(ARGS)

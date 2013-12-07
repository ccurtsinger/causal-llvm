ROOT = ../..
TARGETS = work_queue
LIBS = pthread

include $(ROOT)/common.mk

test:: work_queue
	$(PRELOAD_VAR)=$(ROOT)/libcausal.$(SHLIB_SUFFIX) ./work_queue $(ARGS)

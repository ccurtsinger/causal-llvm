ROOT = ../..
TARGETS = producer_consumer
LIBS = pthread
ARGS = 

include $(ROOT)/common.mk

test:: producer_consumer
	$(PRELOAD_VAR)=$(ROOT)/libcausal.$(SHLIB_SUFFIX) ./producer_consumer $(ARGS)

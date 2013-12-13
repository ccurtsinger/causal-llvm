ROOT = ../..
TARGETS = pca
LIBS = pthread
ARGS = > /dev/null

include $(ROOT)/common.mk

test:: pca
	$(PRELOAD_VAR)=$(ROOT)/libcausal.$(SHLIB_SUFFIX) ./pca $(ARGS)

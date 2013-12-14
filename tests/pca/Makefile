ROOT = ../..
TARGETS = pca
LIBS = pthread dl
ARGS = > /dev/null

include $(ROOT)/common.mk

test:: pca
	$(PRELOAD_VAR)=$(ROOT)/libcausal.$(SHLIB_SUFFIX) ./pca $(ARGS)

ROOT = .
DIRS = lib

include $(ROOT)/common.mk

test: build
	@$(MAKE) -C tests test

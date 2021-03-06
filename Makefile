# Makefile for dnssync

SRC_DIR = src
BIN_DIR = $(DESTDIR)/usr/bin

INSTALL = install

.PHONY: install

all:

compile:
	cd $(SRC_DIR) && make && cd -

install: compile
	if (test ! -d $(BIN_DIR)); then mkdir -p $(BIN_DIR) ; fi
	$(INSTALL) dnssync $(BIN_DIR)
	$(INSTALL) $(SRC_DIR)/dnssync-web $(BIN_DIR)

uninstall:
	rm -f $(BIN_DIR)/dnssync
	rm -f $(BIN_DIR)/dnssync-web

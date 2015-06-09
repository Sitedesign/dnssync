# Makefile for dnssync

SRC_DIR = src
BIN_DIR = $(DESTDIR)/usr/bin
# CNF_DIR = $(DESTDIR)/etc/mynetfilter

INSTALL = install

.PHONY: install

all:

compile:
	cd $(SRC_DIR) && make && cd -

install: compile
	if (test ! -d $(BIN_DIR)); then mkdir -p $(BIN_DIR) ; fi
	$(INSTALL) dnssync $(BIN_DIR)
	$(INSTALL) $(SRC_DIR)/dnssync-web $(BIN_DIR)
#	chmod 700 $(BIN_DIR)/mynetfilter
#	if (test ! -d $(CNF_DIR)); then mkdir -p $(CNF_DIR) ; fi
#	$(INSTALL) iptables.* $(CNF_DIR)
#	$(INSTALL) iptmods.conf $(CNF_DIR)
#	chmod 600 $(CNF_DIR)/*

uninstall:
	rm -f $(BIN_DIR)/dnssync
	rm -f $(BIN_DIR)/dnssync-web

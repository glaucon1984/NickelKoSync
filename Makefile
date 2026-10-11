include NickelHook/NickelHook.mk

override LIBRARY  := src/libkosync.so
override SOURCES  += src/main.cc src/controller.cc src/config.cc src/client.cc src/digest.cc \
                     src/epub.cc src/xpath.cc src/mapping.cc src/nickeldb.cc src/kobalt_state.cc src/nickel.cc src/log.cc \
                     src/settings_dialog.cc
override MOCS     += src/controller.h src/client.h src/settings_dialog.h
override PKGCONF  += Qt5Widgets Qt5Network Qt5Sql zlib
override CFLAGS   += -Wall -Wextra -Werror -fvisibility=hidden
override CXXFLAGS += -Wall -Wextra -Werror -Wno-missing-field-initializers -fvisibility=hidden -fvisibility-inlines-hidden
ifeq ($(KS_CONFIG_DIR),)
override KS_CONFIG_DIR := /mnt/onboard/.adds/kosync
endif
override CPPFLAGS += -Isrc -DKS_CONFIG_DIR='"$(KS_CONFIG_DIR)"'
override KOBOROOT += res/doc:$(KS_CONFIG_DIR)/doc res/config.sample.ini:$(KS_CONFIG_DIR)/config.sample.ini

override SKIPCONFIGURE += strip test
strip:
	$(STRIP) --strip-unneeded src/libkosync.so
.PHONY: strip

# Host build of the pure-Qt units (xpath, epub, digest). Needs a host Qt 5
# (qtbase5-dev) and zlib; see tools/host-test.sh for a container that has them.
HOST_CXX ?= g++
HOST_PKG_CONFIG ?= pkg-config
test:
	$(HOST_CXX) -std=c++11 -fPIC -Wall -Wextra -Werror -Wno-deprecated-declarations -Isrc -DKS_CONFIG_DIR='"/tmp/kosync"' \
	    $(shell $(HOST_PKG_CONFIG) --cflags Qt5Core zlib) \
	    tests/units_test.cc src/xpath.cc src/epub.cc src/digest.cc \
	    $(shell $(HOST_PKG_CONFIG) --libs Qt5Core zlib) -o tests/units_test
	python3 tests/make_fixture.py tests/fixture.epub
	tests/units_test tests/fixture.epub
.PHONY: test

include NickelHook/NickelHook.mk

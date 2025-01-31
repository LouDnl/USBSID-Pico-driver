## Vice Automake file
# Makefile.am for `libusbsiddrv.a'.

AM_CPPFLAGS = \
	@VICE_CPPFLAGS@ \
	@ARCH_INCLUDES@ \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src

AM_CFLAGS = @VICE_CFLAGS@

AM_CXXFLAGS = @VICE_CXXFLAGS@

AM_LDFLAGS = @VICE_LDFLAGS@


noinst_LIBRARIES = libusbsiddrv.a

USBSIDDRIVER=

# Windows drivers
if WINDOWS_COMPILE
if HAVE_USBSID
USBSIDDRIVER += \
	USBSID.o \
	USBSIDInterface.o
endif HAVE_USBSID
endif WINDOWS_COMPILE

if UNIX_COMPILE
if HAVE_USBSID
USBSIDDRIVER += \
	USBSID.o \
	USBSIDInterface.o
endif HAVE_USBSID
endif UNIX_COMPILE


# Built depending on configure options:
EXTRA_libusbsiddrv_a_SOURCES = \
	USBSIDInterface.cpp \
	USBSID.cpp

EXTRA_DIST = \
	USBSID.h \
	USBSIDInterface.h

# Always built:
# (needs to be declared empty to avoid `make` to try to build libusbsiddrv.o
# from libusbsiddrv.c)
libusbsiddrv_a_SOURCES =

# Make sure we actually link the generated object files into the library:
libusbsiddrv_a_LIBADD = $(USBSIDDRIVER)

bin_PROGRAMS     = dpaste
noinst_LIBRARIES = libdpaste.a

libdpaste_a_SOURCES = \
					  node.cpp \
					  conf.cpp \
					  http_client.cpp \
					  bin.cpp \
					  log.cpp \
					  cipher.cpp \
					  gpgcrypto.cpp
dpaste_SOURCES = main.cpp

# Variables defined in toplevel Makefile. Thus, `make` cannot be called from
# this directory.
libdpaste_a_CPPFLAGS = $(dpaste_CPPFLAGS_)
dpaste_CPPFLAGS      = $(dpaste_CPPFLAGS_)
dpaste_LDADD         = libdpaste.a $(dpaste_LIBS)

#  vim: set ts=4 sw=4 tw=120 noet :


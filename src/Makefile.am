bin_PROGRAMS = dpaste

dpaste_SOURCES = \
				 main.cpp \
				 node.cpp \
				 conf.cpp \
				 http_client.cpp \
				 bin.cpp \
				 log.cpp \
				 gpgcrypto.cpp

# Variables defined in toplevel Makefile. Thus, `make` cannot be called from
# this directory.
dpaste_CPPFLAGS = $(dpaste_CPPFLAGS_)
dpaste_LDADD    = $(dpaste_LIBS)

#  vim: set ts=4 sw=4 tw=120 noet :


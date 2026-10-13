check_PROGRAMS = dptest

dptest_SOURCES = \
				 tests.cpp \
				 bin.cpp \
				 node.cpp \
				 conf.cpp \
				 aes.cpp \
				 http_client.cpp

# Variables defined in toplevel Makefile. Thus, `make check` cannot be called
# from this directory.
dptest_CPPFLAGS = -I../src $(dpaste_CPPFLAGS_) $(CATCH2_CFLAGS)
dptest_LDFLAGS  = -L../src
dptest_LDADD    = -ldpaste $(dpaste_LIBS) $(CATCH2_LIBS)

#  vim: set ts=4 sw=4 tw=120 noet :


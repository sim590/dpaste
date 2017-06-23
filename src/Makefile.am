bin_PROGRAMS = dpaste

dpaste_SOURCES = \
				 main.cpp \
				 node.cpp \
				 conf.cpp

dpaste_CPPFLAGS = $(GLIBMM_CFLAGS)
dpaste_LDADD  = -lopendht -lgnutls -lnettle $(GLIBMM_LIBS)

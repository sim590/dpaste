bin_PROGRAMS = dpaste

dpaste_SOURCES = \
				 main.cpp \
				 node.cpp \
				 conf.cpp

dpaste_CPPFLAGS = $(GLIBMM_CFLAGS) $(CURLPP_CLFAGS)
dpaste_LDADD    = -lopendht -lgnutls -lnettle $(GLIBMM_LIBS) $(CURLPP_LIBS) -lb64


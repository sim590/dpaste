bin_PROGRAMS = dpaste

dpaste_SOURCES = \
				 main.cpp \
				 node.cpp \
				 conf.cpp \
				 http_client.cpp

dpaste_CPPFLAGS = $(GLIBMM_CFLAGS) $(CURLPP_CLFAGS)
dpaste_LDADD    = $(OpenDHT_LIBS) $(GLIBMM_LIBS) $(CURLPP_LIBS) -lb64


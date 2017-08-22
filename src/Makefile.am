bin_PROGRAMS = dpaste

dpaste_SOURCES = \
				 main.cpp \
				 node.cpp \
				 conf.cpp \
				 http_client.cpp \
				 bin.cpp \
				 gpgcrypto.cpp

dpaste_CPPFLAGS = $(GLIBMM_CFLAGS) $(CURLPP_CLFAGS) $(GPGME_CFLAGS)
dpaste_LDADD    = $(OpenDHT_LIBS) $(GLIBMM_LIBS) $(CURLPP_LIBS) -lb64 -lgpgmepp $(GPGME_LIBS)


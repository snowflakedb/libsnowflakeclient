
#make LIB=libtelemetry.a

$(LIB): oobtelemetry.c  jsonutil.c sf_ocsp_telemetry_data.c oobtelemetry.h jsonutil.h sf_ocsp_telemetry_data.h
	 @ $(CC) -g $(CFLAGS) -fPIC -c oobtelemetry.c -I$(OOB_SOURCE_DIR)/../$(CURL_DIR)/include -I$(OOB_SOURCE_DIR)/../../include -I$(OOB_SOURCE_DIR)/../util-linux/libuuid/src
	 @ $(CC) -g $(CFLAGS) -fPIC -c jsonutil.c     -I$(OOB_SOURCE_DIR)/../$(CURL_DIR)/lib/vtls -I$(OOB_SOURCE_DIR)/../ -I$(OOB_SOURCE_DIR)/../../include -I$(OOB_SOURCE_DIR)/../util-linux/libuuid/src
	 @ $(CC) -g $(CFLAGS) -fPIC -c sf_ocsp_telemetry_data.c -I$(OOB_SOURCE_DIR)/ -I$(OOB_SOURCE_DIR)/../../include
	 @ $(INFO Building OOB telemetry lib)
	   $(AR) $(AROPTIONS) $@ oobtelemetry.o jsonutil.o sf_ocsp_telemetry_data.o

distclean: clean
	@ rm -f *.a

clean:
	@ rm -f *.o sampleoobevent

sample: 
	$(CC) $(CFLAGS) -g sample.c $(OOB_SOURCE_DIR)/../../lib/cJSON.c  \
          -I$(OOB_SOURCE_DIR)/../../lib/ \
          -I$(OOB_SOURCE_DIR)/../$(CURL_DIR)/lib/vtls \
          -I$(OOB_SOURCE_DIR)/../../include \
          -I$(OOB_SOURCE_DIR)/../$(CURL_DIR)/include \
          libtelemetry.a \
          -o sampleoobevent \
          $(LDFLAGS) $(OOB_SOURCE_DIR)/../../deps-build/linux/Debug/curl/lib/libcurl.a \
          $(OOB_SOURCE_DIR)/../../deps-build/linux/Debug/openssl/lib/libssl.a \
          $(OOB_SOURCE_DIR)/../../deps-build/linux/Debug/openssl/lib/libcrypto.a \
          -lpthread \
          $(OOB_SOURCE_DIR)/../../deps-build/linux/Debug/zlib/lib/libz.a \
          $(OOB_SOURCE_DIR)/../../deps-build/linux/Debug/uuid/lib/libuuid.a \
          -ldl -lrt


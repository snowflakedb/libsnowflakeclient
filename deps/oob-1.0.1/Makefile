
#make LIB=libtelemetry.a

$(LIB): oobtelemetry.c  jsonutil.c oobtelemetry.h jsonutil.h
	 @ $(CC) -g $(CFLAGS) -fPIC -c oobtelemetry.c -I$(OOB_SOURCE_DIR)/../$(CURL_DIR)/include -I$(OOB_SOURCE_DIR)/../$(CURL_DIR)/lib/vtls -I$(OOB_SOURCE_DIR)/../../include -I$(OOB_SOURCE_DIR)/../util-linux/libuuid/src
	 @ $(CC) -g $(CFLAGS) -fPIC -c jsonutil.c     -I$(OOB_SOURCE_DIR)/../$(CURL_DIR)/lib/vtls -I$(OOB_SOURCE_DIR)/../ -I$(OOB_SOURCE_DIR)/../../include -I$(OOB_SOURCE_DIR)/../util-linux/libuuid/src
	 @ $(INFO Building OOB telemetry lib)
	   $(AR) $(AROPTIONS) $@ oobtelemetry.o jsonutil.o

distclean: clean
	@ rm -f *.a

clean:
	@ rm -f *.o sampleoobevent

sample: 
	$(CC) $(CFLAGS) -g sample.c $(LIBS) -o sampleoobevent $(LDFLAGS) 



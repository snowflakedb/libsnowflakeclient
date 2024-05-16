libuuid.a: libuuid/src/predefined.c \
           libuuid/src/unpack.c \
           libuuid/src/unparse.c \
           libuuid/src/clear.c \
           libuuid/src/compare.c \
           libuuid/src/copy.c \
           libuuid/src/gen_uuid.c \
           libuuid/src/isnull.c \
           libuuid/src/pack.c \
           libuuid/src/parse.c \
           libuuid/src/uuid_time.c \
           lib/randutils.c \
           lib/md5.c \
           lib/sha1.c
	 @ $(CC) -g $(CFLAGS) -c libuuid/src/predefined.c -I./libuuid/src -I./include
	 @ $(CC) -g $(CFLAGS) -c libuuid/src/unpack.c -I./libuuid/src -I./include
	 @ $(CC) -g $(CFLAGS) -c libuuid/src/unparse.c -I./libuuid/src -I./include
	 @ $(CC) -g $(CFLAGS) -c libuuid/src/clear.c -I./libuuid/src -I./include
	 @ $(CC) -g $(CFLAGS) -c libuuid/src/compare.c -I./libuuid/src -I./include
	 @ $(CC) -g $(CFLAGS) -c libuuid/src/copy.c -I./libuuid/src -I./include
	 @ $(CC) -g $(CFLAGS) -c libuuid/src/gen_uuid.c -I./libuuid/src -I./include
	 @ $(CC) -g $(CFLAGS) -c libuuid/src/isnull.c -I./libuuid/src -I./include
	 @ $(CC) -g $(CFLAGS) -c libuuid/src/pack.c -I./libuuid/src -I./include
	 @ $(CC) -g $(CFLAGS) -c libuuid/src/parse.c -I./libuuid/src -I./include
	 @ $(CC) -g $(CFLAGS) -c libuuid/src/uuid_time.c -I./libuuid/src -I./include
	 @ $(CC) -g $(CFLAGS) -c lib/randutils.c -I./libuuid/src -I./include
	 @ $(CC) -g $(CFLAGS) -c lib/md5.c -I./libuuid/src -I./include
	 @ $(CC) -g $(CFLAGS) -c lib/sha1.c -I./libuuid/src -I./include
	 @ $(INFO Building uuid lib)
	   $(AR) rcs $@ predefined.o \
                    unpack.o \
                    unparse.o \
                    clear.o \
                    compare.o \
                    copy.o \
                    gen_uuid.o \
                    isnull.o \
                    pack.o \
                    parse.o \
                    uuid_time.o \
                    randutils.o \
                    md5.o \
                    sha1.o

distclean: clean
	@ rm -f *.a

clean:
	@ rm -f *.o


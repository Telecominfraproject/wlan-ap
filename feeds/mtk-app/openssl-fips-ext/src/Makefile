CFLAGS = $(OPENSSL_INCS_LOCATION)
LDFLAGS = $(OPENSSL_LIBS_LOCATION) -lssl -lcrypto -ldl -lpthread

all: main.out hmac_sha

hmac_sha: hmac_sha.c
	$(CC) $(CFLAGS) $(CRYPTODEV_INCS) -o hmac_sha hmac_sha.c

main.out: common.o main.o aesgcm.o aesccm.o
	$(CC) $(CFLAGS) -o openssl-fips-ext $^ $(LDFLAGS)

main.o: main.c
	$(CC) $(CFLAGS) -c -o main.o $< $(LDFLAGS)
aesgcm.o: aesgcm.c
	$(CC) $(CFLAGS) -c -o aesgcm.o $< $(LDFLAGS)

aesccm.o: aesccm.c
	$(CC) $(CFLAGS) -c -o aesccm.o $< $(LDFLAGS)

common.o: common.c
	$(CC) $(CFLAGS) -c -o common.o $< $(LDFLAGS)

clean:
	$(RM) openssl-fips-ext *.o

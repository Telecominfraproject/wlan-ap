/*
 * Demo on how to use /dev/crypto device for HMAC.
 *
 * Placed under public domain.
 *
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <crypto/cryptodev.h>

#define ARRAY_SIZE(ar) (sizeof(ar)/sizeof(ar[0]))

struct hash_data {
	uint8_t *key;
	uint32_t key_len;
	uint8_t *msg;
	uint32_t msg_len;
};

struct algorithm {
	int id;
	char name[20];
	int output_len;
	int is_key;
};

struct algorithm map[] = {
	{CRYPTO_SHA1_HMAC, "hmac-sha1", 20, 1},
	{CRYPTO_SHA2_256_HMAC, "hmac-sha256", 32, 1},
	{CRYPTO_SHA2_384_HMAC, "hmac-sha384", 48, 1},
	{CRYPTO_SHA2_512_HMAC, "hmac-sha512", 64, 1},
	{CRYPTO_SHA1, "sha1", 20, 0},
	{CRYPTO_SHA2_256, "sha256", 32, 0},
	{CRYPTO_SHA2_384, "sha384", 48, 0},
	{CRYPTO_SHA2_512, "sha512", 64, 0}
};

static uint8_t *hex2bin(char *str, uint32_t size)
{
	uint32_t str_len = strlen(str);
	uint32_t byte_len = str_len / 2;
	uint8_t *bytes = (uint8_t *)malloc(byte_len);

	for (int i = 0, j = 0; i < byte_len; i++, j += 2)
		if (sscanf(&str[j], "%2hhx", &bytes[i]) < 0)
			return NULL;

	return bytes;
}

static int run(int cfd, struct hash_data input_data, int algo, int len, int is_key)
{
	struct session_op sess;
	struct crypt_op cryp;
	uint8_t mac[64];

	memset(&sess, 0, sizeof(sess));
	memset(&cryp, 0, sizeof(cryp));
	memset(mac, 0, sizeof(mac));

	sess.cipher = 0;
	sess.mac = algo;
	printf("%d\n", algo);
	if (is_key) {
		sess.mackey = input_data.key;
		sess.mackeylen = input_data.key_len;
	}
	if (ioctl(cfd, CIOCGSESSION, &sess)) {
		perror("ioctl(CIOCGSESSION)");
		return 1;
	}

	cryp.ses = sess.ses;
	cryp.len = input_data.msg_len;
	cryp.src = input_data.msg;
	cryp.mac = mac;
	cryp.op = COP_ENCRYPT;
	if (ioctl(cfd, CIOCCRYPT, &cryp)) {
		perror("ioctl(CIOCCRYPT)");
		return 1;
	}

	printf("result = ");
	for (int i = 0; i < len; i++)
		printf("%.2x", (uint8_t)mac[i]);
	puts("\n");

	/* Finish crypto session */
	if (ioctl(cfd, CIOCFSESSION, &sess.ses)) {
		perror("ioctl(CIOCFSESSION)");
		return 1;
	}

	return 0;
}



void usage(void)
{
	printf(
		"sha and hmac tool:\n"
		"sha Operations:\n"
		"-m              - message\n"
		"select algo: sha1 | sha256 | sha384 | sha512\n"
		"hmac Operation:\n"
		"-m              - message\n"
		"-k key(hex)     - key in hex (must)\n"
		"select algo: hmac-sha1 | hmac-sha256 | hmac-sha384 | hmac-sha512\n");
}

void release(int fd, int cfd, struct hash_data input_data, int is_key)
{
	if (input_data.msg != NULL)
		free(input_data.msg);
	if (input_data.key != NULL && is_key)
		free(input_data.key);
	if (close(cfd))
		perror("close(cfd)");
	if (close(fd))
		perror("close(fd)");
}

int open_node(int *fd, int *cfd)
{
	/* Open the crypto device */
	*fd = open("/dev/crypto", O_RDWR, 0);
	if (*fd < 0) {
		perror("open(/dev/crypto)");
		return 1;
	}

	/* Clone file descriptor */
	if (ioctl(*fd, CRIOGET, cfd)) {
		perror("ioctl(CRIOGET)");
		return 1;
	}

	/* Set close-on-exec (not really neede here) */
	if (fcntl(*cfd, F_SETFD, 1) == -1) {
		perror("fcntl(F_SETFD)");
		return 1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	int fd = -1, cfd = -1;
	int opt;
	int index = -1;
	struct hash_data input_data;

	memset(&input_data, 0, sizeof(input_data));
	while ((opt = getopt(argc, argv, "m:k:")) > 0) {
		switch (opt) {
		case 'm':
			input_data.msg = hex2bin(optarg, strlen(optarg));
			input_data.msg_len = strlen(optarg) / 2;
			break;
		case 'k':
			input_data.key = hex2bin(optarg, strlen(optarg));
			input_data.key_len = strlen(optarg) / 2;
			break;
		case '?':
			usage();
		default:
			break;
		}

	}

	if (argc > optind) {
		for (int i = 0; i < ARRAY_SIZE(map); i++) {
			if (strncmp(map[i].name, argv[optind], strlen(map[i].name)) == 0) {
				index = i;
				break;
			}
		}
	} else if (index == -1 || argc <= optind) {
		printf("select algorithm\n");
		usage();
		return 0;
	}

	if (open_node(&fd, &cfd)) {
		printf("open_node failed\n");
		return 1;
	}

	if (run(cfd, input_data, map[index].id, map[index].output_len, map[index].is_key)) {
		printf("run error\n");
		return 1;
	}

	release(fd, cfd, input_data, map[index].is_key);

	return 0;
}

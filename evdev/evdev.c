#define _GNU_SOURCE /* for asprintf */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <linux/input.h>

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array)	((array[LONG(bit)] >> OFF(bit)) & 1)

#define DEV_INPUT_EVENT "/dev/input"
#define EVENT_DEV_NAME "event"

int is_event_device(const struct dirent *dir) {
	return strncmp(EVENT_DEV_NAME, dir->d_name, 5) == 0;
}

char* scan_devices(void) {
	struct dirent **namelist;
	int i, ndev, devnum;
	char *filename;

	ndev = scandir(DEV_INPUT_EVENT, &namelist, is_event_device, alphasort);
	if (ndev <= 0) {
		return NULL;
	}

	printf("Available devices:\n");

	for (i = 0; i < ndev; i++) {
		char fname[64];
		int fd = -1;
		char name[256] = "???";

		snprintf(fname, sizeof(fname), "%s/%s", DEV_INPUT_EVENT, namelist[i]->d_name);
		fd = open(fname, O_RDONLY);
		if (fd >= 0) {
			ioctl(fd, EVIOCGNAME(sizeof(name)), name);
			close(fd);
		}
		printf("%s:  %s\n", fname, name);
		free(namelist[i]);
	}

	fprintf(stderr, "Select the device event number [0-%d]: ", ndev - 1);
	scanf("%d", &devnum);

	if (devnum >= ndev || devnum < 0) {
		return NULL;
	}

	asprintf(&filename, "%s/%s%d", DEV_INPUT_EVENT, EVENT_DEV_NAME, devnum);
	return filename;
}

static int print_device_info(int fd) {
	int i, j;
	int version;
	unsigned short id[4];
	unsigned long bit[EV_MAX][NBITS(KEY_MAX)];

	if (ioctl(fd, EVIOCGVERSION, &version)) {
		perror("can't get version");
		return 1;
	}
	printf("Input driver version is %d.%d.%d\n", 
	       version >> 16, (version >> 8) & 0xff, version & 0xff);

	ioctl(fd, EVIOCGID, id);
	printf("Input device ID: bus 0x%x vendor 0x%x product 0x%x version 0x%x\n",
		id[ID_BUS], id[ID_VENDOR], id[ID_PRODUCT], id[ID_VERSION]);

	memset(bit, 0, sizeof(bit));
	ioctl(fd, EVIOCGBIT(0, EV_MAX), bit[0]);
	printf("Supported events:\n");
	for (i = 0; i < EV_MAX; i++) {
 		if (test_bit(i, bit[0])) {
			printf("  Event type %d\n", i);
			if (!i) continue;
			ioctl(fd, EVIOCGBIT(i, KEY_MAX), bit[i]);
			for (j = 0; j < KEY_MAX; j++) {
				if (test_bit(j, bit[i])) {
					printf("%d, ", j);
				}
			}
			printf("\n");
		}
	}
	return 0;
}

int print_events(int fd) {
	struct input_event ev;
	unsigned int size;

	printf("Testing ... (interrupt to exit)\n");

	while (1) {
		size = read(fd, &ev, sizeof(struct input_event));

		if (size < sizeof(struct input_event)) {
			printf("expected %u bytes, got %u\n", sizeof(struct input_event), size);
			perror("\nerror reading");
			return EXIT_FAILURE;
		}

		printf("Event: time %ld.%06ld, ", ev.time.tv_sec, ev.time.tv_usec);
		printf("type: %i, code: %i, value: %i\n", ev.type, ev.code, ev.value);
	}
}

int main () {
	int fd, grabbed;
	char *filename;

	if (getuid() != 0) {
		fprintf(stderr, "Not running as root, no devices may be available.\n");
	}

	filename = scan_devices();
	if (!filename) {
		fprintf(stderr, "Device not found\n");
		return EXIT_FAILURE;
	}

	if ((fd = open(filename, O_RDONLY)) < 0) {
		perror("");
		if (errno == EACCES && getuid() != 0) {
			fprintf(stderr, "You do not have access to %s. Try "
					"running as root instead.\n", filename);
		}
		return EXIT_FAILURE;
	}

	free(filename);

	if (print_device_info(fd)) {
		return EXIT_FAILURE;
	}

	grabbed = ioctl(fd, EVIOCGRAB, (void *) 1);
	ioctl(fd, EVIOCGRAB, (void *) 0);
	if (grabbed) {
		printf("This device is grabbed by another process. Try switching VT.\n");
		return EXIT_FAILURE;
	}

	return print_events(fd);
}


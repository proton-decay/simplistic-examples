#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <asm/types.h>          /* for videodev2.h */

#include <linux/videodev2.h>

#define DEVNAME "/dev/video0"

struct buffer {
	void *start;
	size_t length;
};

static int fd_camera = -1;
struct buffer *buffers = NULL;
unsigned int n_buffers = 0;
unsigned int width = 640;
unsigned int height = 480;


/* Copyright 2007 (c) Logitech. All Rights Reserved. (yuv -> rgb conversion) */
int convert_yuv_to_rgb_pixel(int y, int u, int v) {
	unsigned int pixel32 = 0;
	unsigned char *pixel = (unsigned char *) &pixel32;
	int r, g, b;

	r = y + (1.370705 * (v-128));
	g = y - (0.698001 * (v-128)) - (0.337633 * (u-128));
	b = y + (1.732446 * (u-128));

	if(r > 255) r = 255;
	if(g > 255) g = 255;
	if(b > 255) b = 255;
	if(r < 0) r = 0;
	if(g < 0) g = 0;
	if(b < 0) b = 0;

	pixel[0] = r * 220 / 256;
	pixel[1] = g * 220 / 256;
	pixel[2] = b * 220 / 256;

	return pixel32;
}

int convert_yuv_to_rgb_buffer(unsigned char *yuv,
                              unsigned char *rgb,
                              unsigned int width,
                              unsigned int height) {
	unsigned int in, out = 0;
	unsigned int pixel_16;
	unsigned char pixel_24[3];
	unsigned int pixel32;
	int y0, u, y1, v;

	for(in = 0; in < width * height * 2; in += 4) {
		pixel_16 =
			yuv[in + 3] << 24 |
			yuv[in + 2] << 16 |
			yuv[in + 1] <<  8 |
			yuv[in + 0];

		y0 = (pixel_16 & 0x000000ff);
		u  = (pixel_16 & 0x0000ff00) >>  8;
		y1 = (pixel_16 & 0x00ff0000) >> 16;
		v  = (pixel_16 & 0xff000000) >> 24;

		pixel32 = convert_yuv_to_rgb_pixel(y0, u, v);
		pixel_24[0] = (pixel32 & 0x000000ff);
		pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
		pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;

		rgb[out++] = pixel_24[0];
		rgb[out++] = pixel_24[1];
		rgb[out++] = pixel_24[2];

		pixel32 = convert_yuv_to_rgb_pixel(y1, u, v);
		pixel_24[0] = (pixel32 & 0x000000ff);
		pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
		pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;

		rgb[out++] = pixel_24[0];
		rgb[out++] = pixel_24[1];
		rgb[out++] = pixel_24[2];
	}

	return 0;
}

int make_ppm(uint8_t *yuv_buffer, const char *file_name,
             uint32_t width, uint32_t height) {
	unsigned int i;
	unsigned char *rgb_buffer;
	FILE *fd;

	fd = fopen(file_name, "wb");
	if(fd == NULL) {
		perror("make_ppm");
		return -1;
	}
	
	rgb_buffer = (uint8_t *) malloc(3 * width * height);
	convert_yuv_to_rgb_buffer((unsigned char *) yuv_buffer,
	                           rgb_buffer, width, height);

	fprintf(fd, "P3\n%d %d\n255\n", width, height);
	for (i = 0; i < 3*height*width; ++i) {
		fprintf(fd, "%i\n", rgb_buffer[i]);
	}

	free(rgb_buffer);
	fclose(fd);
	return 0;
}

int read_frame(void) {
	struct v4l2_buffer buf;

	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	if (ioctl(fd_camera, VIDIOC_DQBUF, &buf) != 0) {
		if (errno != EAGAIN && errno != EIO) {
			perror("red_frame: VIDIOC_DQBUF");
			exit(EXIT_FAILURE);
		} else {
			return 0;
		}
	}

	if (buf.index > n_buffers) {
		fprintf(stderr, "Wrong buffer size\n");
		exit(EXIT_FAILURE);
	}

	make_ppm(buffers[buf.index].start, "image.ppm", width, height);

	if (ioctl(fd_camera, VIDIOC_QBUF, &buf) != 0) {
		perror("read_frame: VIDIOC_QBUF");
		exit(EXIT_FAILURE);
	}

	return 1;
}

void open_device(void) {
	struct stat st;

	if (stat(DEVNAME, &st) != 0) {
		perror("Cannot identify device");
		exit(EXIT_FAILURE);
	}

	if (!S_ISCHR(st.st_mode)) {
		fprintf(stderr, DEVNAME " is no device\n");
		exit(EXIT_FAILURE);
	}

	fd_camera = open(DEVNAME, O_RDWR | O_NONBLOCK, 0);
	if (fd_camera < 0) {
		perror("Cannot open " DEVNAME);
		exit(EXIT_FAILURE);
	}
}

void init_device(void) {
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;

	if (ioctl(fd_camera, VIDIOC_QUERYCAP, &cap) != 0) {
		perror("VIDIOC_QUERYCAP");
		exit(EXIT_FAILURE);
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf(stderr, DEVNAME " is no video capture device\n");
		exit(EXIT_FAILURE);
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf(stderr, DEVNAME " does not support streaming i/o\n");
		exit(EXIT_FAILURE);
	}

	memset(&cropcap, 0, sizeof(cropcap));
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (ioctl(fd_camera, VIDIOC_CROPCAP, &cropcap) == 0) {
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect;

		if (ioctl(fd_camera, VIDIOC_S_CROP, &crop) != 0) {
			/* Errors ignored. */
		}
	} else {
		/* Errors ignored. */
	}

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = width;
	fmt.fmt.pix.height = height;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

	if (ioctl(fd_camera, VIDIOC_S_FMT, &fmt) != 0) {
		perror("init_device: VIDIOC_S_FMT");
	} /* Note VIDIOC_S_FMT may change width and height. */

	if (fmt.fmt.pix.width != width) {
		width = fmt.fmt.pix.width;
	}
	if (fmt.fmt.pix.height != height) {
		height = fmt.fmt.pix.height;
	}
}

void init_mmap(void) {
	unsigned int i;
	struct v4l2_requestbuffers req;

	memset(&req, 0, sizeof(req));
	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (ioctl(fd_camera, VIDIOC_REQBUFS, &req) != 0) {
		perror("init_mmap: VIDIOC_REQBUFS");
		exit(EXIT_FAILURE);
	}

	if (req.count < 2) {
		fprintf(stderr, "Insufficient buffer memory on " DEVNAME "\n");
		exit(EXIT_FAILURE);
	}
	n_buffers = req.count;

	buffers = calloc(req.count, sizeof(*buffers));
	if (!buffers) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < n_buffers; ++i) {
		struct v4l2_buffer buf;

		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (ioctl(fd_camera, VIDIOC_QUERYBUF, &buf) != 0) {
			perror("VIDIOC_QUERYBUF");
			exit(EXIT_FAILURE);
		}

		buffers[i].length = buf.length;
		buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
		                        MAP_SHARED, fd_camera, buf.m.offset);

		if (buffers[i].start == MAP_FAILED) {
			perror("mmap");
			exit(EXIT_FAILURE);
		}
	}
}

void start_capturing(void) {
	unsigned int i;
	enum v4l2_buf_type type;

	for (i = 0; i < n_buffers; ++i) {
		struct v4l2_buffer buf;

		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (ioctl(fd_camera, VIDIOC_QBUF, &buf) != 0) {
			perror("start_capturing: VIDIOC_QBUF");
			exit(EXIT_FAILURE);
		}
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd_camera, VIDIOC_STREAMON, &type) != 0) {
		perror("start_capturing: VIDIOC_STREAMON");
		exit(EXIT_FAILURE);
	}
}

void mainloop(void) {
	int i;
	for (i = 0; i < 10; ++i) {
		while (1) {
			fd_set fds;
			struct timeval tv;
			int r;

			FD_ZERO(&fds);
			FD_SET(fd_camera, &fds);

			tv.tv_sec = 2;
			tv.tv_usec = 0;

			r = select(fd_camera + 1, &fds, NULL, NULL, &tv);
			if (r == -1) {
				if (EINTR == errno) {
					continue;
				}
				perror("select");
				exit(EXIT_FAILURE);
			} else if (r == 0) {
				fprintf(stderr, "select timeout\n");
				exit(EXIT_FAILURE);
			}

			if (read_frame()) {
				break;
			}
		}
	}
}

void stop_capturing(void) {
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd_camera, VIDIOC_STREAMOFF, &type) != 0) {
		perror("stop_capturing: VIDIOC_STREAMOFF");
		exit(EXIT_FAILURE);
	}
}

void uninit_mmap(void) {
	unsigned int i;
	for (i = 0; i < n_buffers; ++i) {
		if (munmap(buffers[i].start, buffers[i].length) != 0) {
			fprintf(stderr, "munmap");
		}
	}
	free(buffers);
}

void close_device(void) {
	if (close(fd_camera) != 0) {
		perror("close");
		exit(EXIT_FAILURE);
	}
}

int main() {
	open_device();
	init_device();
	init_mmap();

	start_capturing();
	mainloop();
	stop_capturing();

	uninit_mmap();
	close_device();

	return EXIT_SUCCESS;
}

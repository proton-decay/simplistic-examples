#include <math.h>
#include <inttypes.h>
#include <alsa/asoundlib.h>

snd_pcm_uframes_t period_size;

typedef double (*SndGenFunc)(double);

unsigned int rate = 48000;

double sndgen_sin(double t) {
	static double k = 2.0 * M_PI * 440.0;
	return sin(k * t);
}

double sndgen_beat(double t) {
	static double k = 2.0 * M_PI * 440.0;
	static double l = 2.0 * M_PI * 441.0;
	return sin(k * t) + sin(l * t);
}

double sndgen_two(double t) {
	static double k = 2.0 * M_PI * 300.0;
	static double l = 2.0 * M_PI * 500.0;
	return sin(k * t) + sin(l * t);
}

void sndgen(snd_pcm_t *handle, SndGenFunc genfunc, unsigned int len) {
	int err;
	unsigned int i, j, k;
	double t = 0;
	double dt = 1.0 / rate;
	uint8_t *period;
	period = malloc(snd_pcm_frames_to_bytes(handle, period_size));
	if (period == NULL) {
		fprintf(stderr, "No enough memory\n");
		exit(EXIT_FAILURE);
	}
	for (i = 0; i < len; i++) {
		double periods = rate / period_size; /* Periods per second */
		for(j = 0; j < periods; j++) {
			double res;
			int16_t *samp = (int16_t*) period;

			for (k = 0; k < period_size; ++k) {
				res = genfunc(t) * 0x03fffffff;
				samp[k] = (int32_t) res >> 16;
				t += dt;
			}

			err = snd_pcm_writei(handle, period, period_size);
			if (err < 0) {
				fprintf(stderr, "Transfer failed: %s\n", snd_strerror(err));
				free(period);
				snd_pcm_close(handle);
				exit(EXIT_FAILURE);
			}
		}
	}
	free(period);
}

static int set_hwparams(snd_pcm_t *handle, snd_pcm_hw_params_t *params) {
	int err;
	unsigned int exact_rate = rate;

	/* Choose all parameters */
	err = snd_pcm_hw_params_any(handle, params);
	if (err < 0) {
		fprintf(stderr, "Broken configuration for playback: %s\n", snd_strerror(err));
		return err;
	}

	/* Set the interleaved read/write format */
	err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) {
		fprintf(stderr, "Error setting access: %s.\n", snd_strerror(err));
		return -1;
	}

	/* Set sample format */
	err = snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16);
	if (err < 0) {
		fprintf(stderr, "Error setting format.\n");
		return -1;
	}

	/* Set number of channels */
	err = snd_pcm_hw_params_set_channels(handle, params, 1);
	if (err < 0) {
		fprintf(stderr, "Error setting channels.\n");
		return -1;
	}

	/* Set sample rate. If the exact rate is not supported */
	/* by the hardware, use nearest possible rate.         */
	err = snd_pcm_hw_params_set_rate_near(handle, params, &rate, 0);
	if (err < 0) {
		fprintf(stderr, "Error setting rate.\n");
		return -1;
	}
	if (rate != exact_rate) {
		fprintf(stderr, "The rate %d Hz is not supported.\n", exact_rate);
		fprintf(stderr, "Using %d Hz instead.\n", rate);
	}

	/* Write the parameters to device */
	err = snd_pcm_hw_params(handle, params);
	if (err < 0) {
		fprintf(stderr, "Unable to set hw params for playback: %s\n", snd_strerror(err));
		return err;
	}

	snd_pcm_hw_params_get_period_size(params, &period_size, NULL);
	return 0;
}

int main() {
	int err;
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *hwparams;

	snd_pcm_hw_params_alloca(&hwparams);

	err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (err < 0) {
		printf("Playback open error: %d,%s\n", err, snd_strerror(err));
		exit(EXIT_FAILURE);
	}

	err = set_hwparams(handle, hwparams);
	if (err < 0) {
		printf("Setting of hwparams failed: %s\n", snd_strerror(err));
		snd_pcm_close(handle);
		exit(EXIT_FAILURE);
	}

	printf("Sine, 440Hz\n");
	sndgen(handle, sndgen_sin,  5);
	printf("Beats, sines 440 and 441Hz\n");
	sndgen(handle, sndgen_beat, 5);
	printf("Sum of sines, 300 and 500Hz\n");
	sndgen(handle, sndgen_two,  5);

	snd_pcm_close(handle);
	exit(EXIT_SUCCESS);
}


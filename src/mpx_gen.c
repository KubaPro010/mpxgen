/*
 * mpxgen - FM multiplex encoder with Stereo and RDS
 * Copyright (C) 2019 Anthony96922
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <signal.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <ao/ao.h>
#include <pthread.h>

#include "rds.h"
#include "fm_mpx.h"
#include "control_pipe.h"
#include "audio_conversion.h"
#include "resampler.h"

int stop_mpx;

void stop() {
	stop_mpx = 1;
}

pthread_t control_pipe_thread;

static void *control_pipe_worker(void *arg) {
	while (!stop_mpx) {
		poll_control_pipe();
		usleep(10000);
	}

	close_control_pipe();
	pthread_exit(NULL);
}


static inline void get_fixed_buffer(float *out, float *in, float *temp_buffer, size_t in_size, size_t wanted_size) {

}

void set_output_ppm(float new_ppm) {
	// no-op
}

int main(int argc, char **argv) {
	int opt;
	char audio_file[51] = {0};
	char *output_file = NULL;
	char *control_pipe = NULL;
	uint8_t rds = 1;
	uint8_t af[MAX_AF+1] = {0};
	uint8_t af_size = 0;
	// Use arrays to enforce max length for RDS text items
	char ps[9] = "Mpxgen";
	char rt[65] = "Mpxgen: FM Stereo and RDS encoder";
	char ptyn[9] = {0};
	char callsign[5] = {0};
	uint16_t pi = 0x1000;
	uint8_t pty = 0;
	uint8_t tp = 0;
	float ppm = 0;
	uint8_t mpx = 50;
	uint8_t wait = 1;

	// SRC
	SRC_STATE *src_state[2] = {0};
	SRC_DATA src_data[2];

	// buffers
	float *mpx_data;
        char *dev_out;
        char *stereo_out;

	float *temp_buffer_in_0;
	float *temp_buffer_in_1;
	float *temp_buffer_in_2;
	float *temp_buffer_in_3;
	float *temp_buffer_in_4;
	float *temp_buffer_out_0;
	float *temp_buffer_out_1;
	float *temp_buffer_out_2;
	float *temp_buffer_out_3;
	float *temp_buffer_out_4;

	pthread_attr_t attr;

	const char	*short_opt = "a:o:m:x:W:R:i:s:r:p:T:A:P:S:C:h";
	struct option	long_opt[] =
	{
		{"audio",	required_argument, NULL, 'a'},
		{"output-file",	required_argument, NULL, 'o'},

		{"mpx",		required_argument, NULL, 'm'},
		{"ppm",		required_argument, NULL, 'x'},
		{"wait",	required_argument, NULL, 'W'},

		{"rds",		required_argument, NULL, 'R'},
		{"pi",		required_argument, NULL, 'i'},
		{"ps",		required_argument, NULL, 's'},
		{"rt",		required_argument, NULL, 'r'},
		{"pty",		required_argument, NULL, 'p'},
		{"tp",		required_argument, NULL, 'T'},
		{"af",		required_argument, NULL, 'A'},
		{"ptyn",	required_argument, NULL, 'P'},
		{"callsign",	required_argument, NULL, 'S'},
		{"ctl",		required_argument, NULL, 'C'},

		{"help",	no_argument, NULL, 'h'},
		{ 0,		0,		0,	0 }
	};

	while((opt = getopt_long(argc, argv, short_opt, long_opt, NULL)) != -1)
	{
		switch(opt)
		{
			case 'a': //audio
				strncpy(audio_file, optarg, 50);
				break;

			case 'o': //output-file
				output_file = optarg;
				break;

			case 'm': //mpx
				mpx = strtoul(optarg, NULL, 10);
				if (mpx < 1 || mpx > 100) {
					fprintf(stderr, "MPX volume must be between 1 - 100.\n");
					return 1;
				}
				break;

			case 'x': //ppm
				ppm = strtof(optarg, NULL);
				break;

			case 'W': //wait
				wait = strtoul(optarg, NULL, 10);
				break;

			case 'R': //rds
				rds = strtoul(optarg, NULL, 10);
				break;

			case 'i': //pi
				pi = strtoul(optarg, NULL, 16);
				break;

			case 's': //ps
				strncpy(ps, optarg, 8);
				break;

			case 'r': //rt
				strncpy(rt, optarg, 64);
				break;

			case 'p': //pty
				pty = strtoul(optarg, NULL, 10);
				break;

			case 'T': //tp
				tp = strtoul(optarg, NULL, 10);
				break;

			case 'A': //af
				af_size++;
				if (af_size > MAX_AF) {
					fprintf(stderr, "AF list is too large.\n");
					return 1;
				}
				uint16_t freq = (uint16_t)(10*strtof(optarg, NULL));
				if (freq < 876 || freq > 1079) {
					fprintf(stderr, "Alternative Frequency has to be set in range of 87.6 MHz - 107.9 MHz\n");
					return 1;
				}
				af[af_size] = freq-875;
				break;

			case 'P': //ptyn
				strncpy(ptyn, optarg, 8);
				break;

			case 'S': //callsign
				strncpy(callsign, optarg, 4);
				break;

			case 'C': //ctl
				control_pipe = optarg;
				break;

			case 'h': //help
			case '?':
			default:
				fprintf(stderr,
					"This is Mpxgen, a lightweight Stereo and RDS encoder.\n"
					"\n"
					"Usage: %s [options]\n"
					"\n"
					"[Audio]\n"
					"\n"
#ifdef ALSA
					"    -a / --audio        Input file, pipe or ALSA capture\n"
#else
					"    -a / --audio        Input file or pipe\n"
#endif
					"    -o / --output-file  PCM out\n"
					"\n"
					"[MPX controls]\n"
					"\n"
					"    -m / --mpx          MPX volume [ default: %d ]\n"
					"    -x / --ppm          Clock drift correction\n"
					"    -W / --wait         Wait for new audio [ default: %d ]\n"
					"\n"
					"[RDS encoder]\n"
					"\n"
					"    -R / --rds          RDS switch [ default: %d ]\n"
					"    -i / --pi           Program Identification code [ default: %04X ]\n"
					"    -s / --ps           Program Service name [ default: \"%s\" ]\n"
					"    -r / --rt           Radio Text [ default: \"%s\" ]\n"
					"    -p / --pty          Program Type [ default: %d ]\n"
					"    -T / --tp           Traffic Program [ default: %d ]\n"
					"    -A / --af           Alternative Frequency (more than one AF may be passed)\n"
					"    -P / --ptyn         PTY Name\n"
					"    -S / --callsign     Callsign to calculate the PI code from (overrides -i/--pi)\n"
					"    -C / --ctl          Control pipe\n"
					"\n",
				argv[0], mpx, wait, rds, pi, ps, rt, pty, tp);
				return 1;
		}
	}

	if (audio_file == NULL && !rds) {
		fprintf(stderr, "Nothing to do. Exiting.\n");
		return 1;
	}

	af[0] = af_size;

	pthread_attr_init(&attr);

	// temp buffers
	temp_buffer_in_0 = malloc(65536 * sizeof(float));
	temp_buffer_in_1 = malloc(65536 * sizeof(float));
	temp_buffer_in_2 = malloc(65536 * sizeof(float));
	temp_buffer_in_3 = malloc(65536 * sizeof(float));
	temp_buffer_in_4 = malloc(65536 * sizeof(float));
	temp_buffer_out_0 = malloc(65536 * sizeof(float));
	temp_buffer_out_1 = malloc(65536 * sizeof(float));
	temp_buffer_out_2 = malloc(65536 * sizeof(float));
	temp_buffer_out_3 = malloc(65536 * sizeof(float));
	temp_buffer_out_4 = malloc(65536 * sizeof(float));

	// Gracefully stop the encoder on SIGINT or SIGTERM
	signal(SIGINT, stop);
	signal(SIGTERM, stop);

	// Data structures for baseband data
	mpx_data = malloc(DATA_SIZE * sizeof(float));
	dev_out = malloc(DATA_SIZE);
	stereo_out = malloc(DATA_SIZE);

	size_t frames;

	// AO
	ao_device *device;
	ao_sample_format format;
	memset(&format, 0, sizeof(format));
	format.channels = 2;
	format.bits = 16;
	format.rate = 192000;
	format.byte_format = AO_FMT_LITTLE;

	ao_initialize();

	if (output_file != NULL) {
		int ao_driver = ao_driver_id("wav");
		if (strcmp(output_file, "-") == 0) {
			ao_driver = ao_driver_id("raw");
			if (isatty(fileno(stdout))) {
				fprintf(stderr, "Not writing audio data to a terminal. Exiting.\n");
				ao_shutdown();
				return -1;
			}
		}
		if ((device = ao_open_file(ao_driver, output_file, 1, &format, NULL)) == NULL) {
			fprintf(stderr, "Error: cannot open output file.\n");
			ao_shutdown();
			return -1;
		}
	} else {
		if ((device = ao_open_live(ao_default_driver_id(), &format, NULL)) == NULL) {
			fprintf(stderr, "Error: cannot open sound device.\n");
			ao_shutdown();
			return -1;
		}
	}


	// SRC in (input -> MPX)
	src_data[0].output_frames = 32768;
	src_data[0].src_ratio = ((double)MPX_SAMPLE_RATE / 192000);
	src_data[0].data_in = temp_buffer_in_0;
	src_data[0].data_out = temp_buffer_in_1;
	src_data[0].end_of_input = 0;

	if ((src_state[0] = resampler_init(2)) == NULL) {
		fprintf(stderr, "Could not create input resampler.\n");
		goto exit;
	}

	// SRC out (MPX -> output)
	src_data[1].output_frames = 32768;
	src_data[1].src_ratio = (192000 / (double)MPX_SAMPLE_RATE) + (ppm / 1e6);
	src_data[1].data_in = temp_buffer_out_0;
	src_data[1].data_out = temp_buffer_out_1;
	src_data[1].end_of_input = 0;

	if ((src_state[1] = resampler_init(2)) == NULL) {
		fprintf(stderr, "Could not create output resampler.\n");
		goto exit;
	}

	// Initialize the baseband generator
	fm_mpx_open();
	set_output_volume(mpx);

	// Initialize the RDS modulator
	if (!rds) set_carrier_volume(1, 0);
	if (init_rds_encoder(pi, ps, rt, pty, tp, af, ptyn, callsign) < 0) goto exit;

	// Initialize the control pipe reader
	if(control_pipe) {
		if(open_control_pipe(control_pipe) == 0) {
			fprintf(stderr, "Reading control commands on %s.\n", control_pipe);
			// Create control pipe polling worker
			if (pthread_create(&control_pipe_thread, &attr, control_pipe_worker, NULL) < 0) {
				fprintf(stderr, "Could not create control pipe thread.\n");
				goto exit;
			}
		} else {
			fprintf(stderr, "Failed to open control pipe: %s.\n", control_pipe);
			control_pipe = NULL;
		}
	}

	pthread_attr_destroy(&attr);

	for (;;) {
		// TODO: run this, SRC and ao_play as separate threads
		if (audio_file[0]) {
			fprintf(stderr, "Input does not work for now.\n");
			break;
			//fm_mpx_get_samples(temp_buffer_out_0);
		} else {
			fm_rds_get_samples(temp_buffer_out_0);
			src_data[1].input_frames = IN_NUM_FRAMES;

			if (resample(src_state[1], src_data[1], &frames) < 0) break;
		}

		float2char(temp_buffer_out_1, dev_out, frames);

		// num_bytes = audio frames * bytes per sample
		if (!ao_play(device, dev_out, frames * sizeof(short))) {
			fprintf(stderr, "Error: could not play audio.\n");
			break;
		}

		if (stop_mpx) {
			fprintf(stderr, "Stopping...\n");
			break;
		}
	}

exit:
	fm_mpx_close();

	ao_close(device);
	ao_shutdown();

	resampler_exit(src_state[0]);
	resampler_exit(src_state[1]);

	free(mpx_data);
	free(dev_out);
	free(stereo_out);

	free(temp_buffer_in_0);
	free(temp_buffer_in_1);
	free(temp_buffer_in_2);
	free(temp_buffer_in_3);
	free(temp_buffer_in_4);
	free(temp_buffer_out_0);
	free(temp_buffer_out_1);
	free(temp_buffer_out_2);
	free(temp_buffer_out_3);
	free(temp_buffer_out_4);

	return 0;
}

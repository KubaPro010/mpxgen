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

#include "common.h"
#include <pulse/simple.h>

static size_t buffer_size;
pa_simple *device;

int8_t open_pulse_input(char *input, uint32_t sample_rate, size_t num_frames) {
	int err;
	pa_sample_spec format;
	format.format = PA_SAMPLE_S16LE;
	format.channels = 2;
	format.rate = sample_rate;

	buffer_size = num_frames;

	device = pa_simple_new(NULL, "mpxgen", PA_STREAM_RECORD, input, "mpxgen", &format, NULL, NULL, NULL);
	if(device == NULL) {
		fprintf(stderr, "Error: failed to open audio device\n");
		return -1;
	}

	return 0;
}

int16_t read_pulse_input(short *buffer) {
	int16_t frames_read;
	uint16_t frames;

	frames_read = pa_simple_read(device, buffer, buffer_size, NULL);
	if (frames_read < 0) {
		fprintf(stderr, "Error: read from audio device failed\n");
		frames = -1;
	} else {
		frames = frames_read;
	}

	return frames;
}

int8_t close_pulse_input() {
	pa_simple_free(device); // This doesn't return any error codes

	return 0;
}

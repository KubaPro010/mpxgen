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

pa_simple *device;
int8_t open_pulse_output(char *output_device, unsigned int sample_rate, unsigned int channels) {
	int err;
	pa_sample_spec format;
	format.format = PA_SAMPLE_S16LE;
	format.channels = channels;
	format.rate = sample_rate;

	device = pa_simple_new(NULL, "mpxgen", PA_STREAM_RECORD, output_device, "mpxgen", &format, NULL, NULL, NULL);
	if(device == NULL) {
		fprintf(stderr, "Error: failed to open audio device\n");
		return -1;
	}

	return 0;
}

int16_t write_pulse_output(short *buffer, size_t frames) {
	int frames_written;
	frames_written = pa_simple_write(device, buffer, frames, NULL);
	return frames_written;
}

int8_t close_pulse_output() {
	int err;

	err = pa_simple_drain(device, NULL);
	if (err < 0) {
		fprintf(stderr, "Error: could not drain sink\n");
		return -1;
	}
	pa_simple_free(device);

	return 0;
}

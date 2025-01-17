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
#include "output.h"

static int output_type;

int open_output(char *output_name, unsigned int sample_rate, unsigned int channels) {
	// TODO: better detect live capture cards
	if (output_name[0] == 'p' && output_name[1] == 'u' &&
	    output_name[2] == 'l' && output_name[3] == 's' &&
	output_name[2] == 'e' && output_name[3] == ':') { // check if name is prefixed with "pulse:"
		output_name = output_name+6; // don't pass prefix
		fprintf(stderr, "Using pulse device \"%s\" for output.\n", output_name);
		if (open_pulse_output(output_name, sample_rate, channels) < 0) {
			fprintf(stderr, "Could not open pulse sink.\n");
			return -1;
		}
		output_type = 2;
	} else {
		fprintf(stderr, "Writing MPX output to \"%s\".\n", output_name);
		if (open_file_output(output_name, sample_rate, channels) < 0) {
			return -1;
		}
		output_type = 1;
	}
	return 1;
}

int write_output(short *audio, size_t frames) {
	if (output_type == 1) {
		if (write_file_output(audio, frames) < 0) return -1;
	}
	if (output_type == 2) {
		if (write_pulse_output(audio, frames) < 0) return -1;
	}

	return 0;
}

void close_output() {
	if (output_type == 1) {
		close_file_output();
	}
	if (output_type == 2) {
		close_pulse_output();
	}
}

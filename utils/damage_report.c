/*
	damage_report: Simple example for mxc_epdc_fb_damage usage.
	Copyright (C) 2021 NiLuJe <ninuje@gmail.com>
	SPDX-License-Identifier: GPL-2.0-only
*/

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "../mxc_epdc_fb_damage.h"

int
    main(void)
{
	int ret = EXIT_SUCCESS;

	// NOTE: This exercises a full NONBLOCK poll + read workflow (with, err, *extensive* error handling),
	//       but you can also do blocking read() calls if that's more your speed ;).
	int fd = open("/dev/fbdamage", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd == -1) {
		perror("open");
		ret = EXIT_FAILURE;
		goto cleanup;
	}

	struct pollfd pfd = { 0 };
	pfd.fd            = fd;
	pfd.events        = POLLIN;

	while (true) {
		int poll_num = poll(&pfd, 1, -1);

		if (poll_num == -1) {
			if (errno == EINTR) {
				continue;
			}

			perror("poll");
			ret = EXIT_FAILURE;
			goto cleanup;
		}

		if (poll_num > 0) {
			if (pfd.revents & POLLIN) {
				mxcfb_damage_update damage = { 0 };

				while (true) {
					ssize_t len = read(fd, &damage, sizeof(damage));

					if (len < 0) {
						if (errno == EINTR) {
							continue;
						} else if (errno == EAGAIN) {
							// Damage ring buffer drained, back to poll!
							break;
						} else {
							perror("read");
							ret = EXIT_FAILURE;
							goto cleanup;
						}
					}

					if (len == 0) {
						// Should never happen
						errno = EPIPE;
						perror("read");
						ret = EXIT_FAILURE;
						goto cleanup;
					}

					if (len != sizeof(damage)) {
						// Should *also* never happen ;p.
						errno = EINVAL;
						perror("read");
						ret = EXIT_FAILURE;
						goto cleanup;
					}

					// Phew, we're good!
					// Start with some timestamps (first, the actual event's timestamp, MONOTONIC)
					printf("[%llu.%.9llu ->",
					       damage.timestamp / NSEC_PER_SEC,
					       damage.timestamp % NSEC_PER_SEC);

					// Then now (REALTIME)
					time_t     t   = time(NULL);
					struct tm* tmp = localtime(&t);
					char       time_str[64];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-y2k"
					if (strftime(time_str, sizeof(time_str), " %x %X] ", tmp) == 0) {
#pragma GCC diagnostic pop
						perror("strftime");
						ret = EXIT_FAILURE;
						goto cleanup;
					}
					fputs(time_str, stdout);

					if (damage.format == DAMAGE_UPDATE_DATA_V1_NTX) {
						fputs("MXCFB_SEND_UPDATE_V1_NTX: ", stdout);
					} else if (damage.format == DAMAGE_UPDATE_DATA_V1) {
						fputs("MXCFB_SEND_UPDATE_V1: ", stdout);
					} else if (damage.format == DAMAGE_UPDATE_DATA_V2) {
						fputs("MXCFB_SEND_UPDATE_V2: ", stdout);
					} else if (damage.format == DAMAGE_UPDATE_DATA_SUNXI_KOBO_DISP2) {
						fputs("DISP_EINK_UPDATE2: ", stdout);
					} else {
						printf("Unknown damage data format: %u!\n", damage.format);
						ret = EXIT_FAILURE;
						goto cleanup;
					}

					// NOTE: We print all the fields, no matter the actual data format (the module ensures they're set to sane defaults).
					printf(
					    "overflow_notify=%d, queue_size=%d {update_region={top=%u, left=%u, width=%u, height=%u}, waveform_mode=%u, update_mode=%u, update_marker=%u, temp=%d, flags=%u, dither_mode=%d, quant_bit=%d, alt_buffer_data={virt_addr=%p, phys_addr=%u, width=%u, height=%u, alt_update_region={top=%u, left=%u, width=%u, height=%u}}}\n",
					    damage.overflow_notify,
					    damage.queue_size,
					    damage.data.update_region.top,
					    damage.data.update_region.left,
					    damage.data.update_region.width,
					    damage.data.update_region.height,
					    damage.data.waveform_mode,
					    damage.data.update_mode,
					    damage.data.update_marker,
					    damage.data.temp,
					    damage.data.flags,
					    damage.data.dither_mode,
					    damage.data.quant_bit,
					    damage.data.alt_buffer_data.virt_addr,
					    damage.data.alt_buffer_data.phys_addr,
					    damage.data.alt_buffer_data.width,
					    damage.data.alt_buffer_data.height,
					    damage.data.alt_buffer_data.alt_update_region.top,
					    damage.data.alt_buffer_data.alt_update_region.left,
					    damage.data.alt_buffer_data.alt_update_region.width,
					    damage.data.alt_buffer_data.alt_update_region.height);
				}
			}
		}
	}

	// Unreachable outside of gotos
cleanup:
	if (fd != -1) {
		close(fd);
	}
	return ret;
}

/* quadcastrgb - set RGB lights of HyperX Quadcast S and DuoCast
 * File devio.h
 * Device input/output.
 * The device is microphone HyperX Quadcast S
 * distinguished by the VID & PID.
 *
 * <----- License notice ----->
 * Copyright (C) 2022, 2023, 2024, 2025 Ors1mer
 *
 * You may contact the author by email:
 * ors1mer [[at]] ors1mer dot xyz
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License ONLY.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see
 * <https://www.gnu.org/licenses/gpl-2.0.en.html>. For any questions
 * concerning the license, you can write to <licensing@fsf.org>.
 * Also, you may visit the Free Software Foundation at
 * 51 Franklin Street, Fifth Floor Boston, MA 02110 USA.
 */
#ifndef DEVIO_SENTRY
#define DEVIO_SENTRY

/* For Quadcast 2S protocol */
#define QS2S_DISPLAY_CODE 0x44
#define QS2S_PACKET_CNT_CODE 0x01
#define QS2S_LED_CNT 108
#define QS2S_SOLID_PKT_CNT 0x06

#include <libusb-1.0/libusb.h>
#include "rgbmodes.h" /* for datpack & byte_t types, count_color_pairs, defs */

#define QUADCAST_2S_PID 0x02b5 /* for rgbmodes */

/* Functions */
libusb_device_handle *open_mic(unsigned short *pid);
void send_packets(libusb_device_handle *handle, const datpack *data_arr,
                  int pck_cnt, int verbose, int vid);
#endif

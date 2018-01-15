/*
 * rtlizer - a simple spectrum analyzer using rtlsdr
 * 
 * Copyright (C) 2013 Alexandru Csete
 * 
 * Includes code from rtl_test.c:
 * Copyright (C) 2012 by Steve Markgraf <steve@steve-m.de>
 *
 * rtlizer is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * rtlizer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libfcd.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdio.h>
#include <rtl-sdr.h>
#include <stdlib.h>
#include <unistd.h>

#include "kiss_fft.h"


#define DEFAULT_SAMPLE_RATE 2048000

#define PORT 5555
#define IP_ADDRESS "192.168.0.4" // can be multicast IP


static uint8_t *buffer;
static uint32_t dev_index = 0;
static uint32_t frequency = 98000000;
static uint32_t samp_rate = DEFAULT_SAMPLE_RATE;
static uint32_t buff_len = 2048;
static int      ppm_error = 0;

static float    lut[256];       /* look-up table to convert U8 to +/- 1.0f */

static int      fft_size = 640;
static kiss_fft_cfg fft_cfg;
static kiss_fft_cpx *fft_in;
static kiss_fft_cpx *fft_out;
static float   *log_pwr_fft;    /* dbFS relative to 1.0 */

static rtlsdr_dev_t *dev = NULL;


static void setup_rtlsdr()
{
    int             device_count;
    int             r;

    buffer = malloc(buff_len * sizeof(uint8_t));

    device_count = rtlsdr_get_device_count();
    if (!device_count)
    {
        fprintf(stderr, "No supported devices found.\n");
        exit(1);
    }

    r = rtlsdr_open(&dev, dev_index);
    if (r < 0)
    {
        fprintf(stderr, "Failed to open rtlsdr device #%d.\n", dev_index);
        exit(1);
    }

    if (ppm_error != 0)
    {
        r = rtlsdr_set_freq_correction(dev, ppm_error);
        if (r < 0)
            fprintf(stderr, "WARNING: Failed to set PPM error.\n");
    }

    r = rtlsdr_set_sample_rate(dev, samp_rate);
    if (r < 0)
        fprintf(stderr, "WARNING: Failed to set sample rate.\n");

    r = rtlsdr_set_center_freq(dev, frequency);
    if (r < 0)
        fprintf(stderr, "WARNING: Failed to set center freq.\n");

    r = rtlsdr_set_tuner_gain_mode(dev, 0);
    if (r < 0)
        fprintf(stderr, "WARNING: Failed to enable automatic gain.\n");

    r = rtlsdr_reset_buffer(dev);
    if (r < 0)
        fprintf(stderr, "WARNING: Failed to reset buffers.\n");

}

static char read_rtlsdr()
{
    char            error = 0;
    int             n_read;
    int             r;

    r = rtlsdr_read_sync(dev, buffer, buff_len, &n_read);
    if (r < 0)
    {
        fprintf(stderr, "WARNING: sync read failed.\n");
        error = 1;
    }

    if ((uint32_t) n_read < buff_len)
    {
        fprintf(stderr, "Short read (%d / %d), samples lost, exiting!\n",
                n_read, buff_len);
        error = 1;
    }

    return error;
}

static void run_fft()
{
    int             i;
    kiss_fft_cpx    pt;
    float           pwr, lpwr;
    float           gain;

    for (i = 0; i < fft_size; i++)
    {
        fft_in[i].r = lut[buffer[2 * i]];
        fft_in[i].i = lut[buffer[2 * i + 1]];
    }
    kiss_fft(fft_cfg, fft_in, fft_out);
    for (i = 0; i < fft_size; i++)
    {
        /* shift, normalize and convert to dBFS */
        if (i < fft_size / 2)
        {
            pt.r = fft_out[fft_size / 2 + i].r / fft_size;
            pt.i = fft_out[fft_size / 2 + i].i / fft_size;
        }
        else
        {
            pt.r = fft_out[i - fft_size / 2].r / fft_size;
            pt.i = fft_out[i - fft_size / 2].i / fft_size;
        }
        pwr = pt.r * pt.r + pt.i * pt.i;
        lpwr = 10.f * log10(pwr + 1.0e-20f);

        gain = 0.3 * (100.f + lpwr) / 100.f;
        log_pwr_fft[i] = log_pwr_fft[i] * (1.f - gain) + lpwr * gain;
    }

}

int main(int argc, char *argv[])
{
    int             opt;
    int             i;
    int             bins = 640;
    
    struct sockaddr_in addr;
    int fd;

    /* parse cmd line */
    while ((opt = getopt(argc, argv, "d:f:p:h")) != -1)
    {
        switch (opt)
        {
        case 'd':
            dev_index = atoi(optarg);
            break;
        case 'f':
            frequency = atoll(optarg);
            break;
        case 'p':
            ppm_error = atoi(optarg);
            break;
        case 'n':
            bins = atoi(optarg);
            break;
        case 'h':
        case '?':
        default:
            printf
                ("usage: rtlizer [-d device_index] [-f frequency [Hz]] [-p ppm_error] [-n bins]\n");
            exit(EXIT_SUCCESS);
            break;
        }
    }

    /* LUT */
    for (i = 0; i < 256; i++)
        lut[i] = (float)i / 127.5f - 1.f;

    /* set up FFT */
    fft_size = 2 * bins / 2;
    fft_cfg = kiss_fft_alloc(fft_size, 0, NULL, NULL);
    fft_in = malloc(bins * sizeof(kiss_fft_cpx));
    fft_out = malloc(bins * sizeof(kiss_fft_cpx));
    log_pwr_fft = malloc(bins * sizeof(float));
    for (i = 0; i < bins; i++)
        log_pwr_fft[i] = -70.f;

    setup_rtlsdr();
    
    if ((fd=socket(AF_INET,SOCK_DGRAM,0)) < 0) {
        perror("socket");
        exit(1);
    }

    /* set up destination address */
    memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=inet_addr(IP_ADDRESS);
    addr.sin_port=htons(PORT);
    
    while(1)
    {
        /* get samples from rtlsdr */
        if (read_rtlsdr())
            return 1;           /* error reading -> exit */

        /* calculate FFT */
        run_fft();


        // transmit log_pwr_fft
        if (sendto(fd, log_pwr_fft, fft_size*sizeof(log_pwr_fft[0]), 0, (struct sockaddr *) &addr, sizeof(addr)) < 0)
        {
            perror("sendto");
            exit(1);
        }
        
        //sleep(1);
        
        // TODO: catch ctrl-C (or any key, really)
    }
    
    rtlsdr_close(dev);
    free(buffer);

    free(fft_cfg);
    free(fft_in);
    free(fft_out);
    free(log_pwr_fft);

    return 0;
}

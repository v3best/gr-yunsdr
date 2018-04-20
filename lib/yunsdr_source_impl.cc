/* -*- c++ -*- */
/*
 * Copyright 2018 V3 Inc.
 *
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

/*
 * config.h is generated by configure.  It contains the results
 * of probing for features, options etc.  It should be the first
 * file included in your .cc file.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdexcept>
#include <iostream>

#include <boost/assign.hpp>
#include <boost/format.hpp>
#include <boost/detail/endian.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/thread/thread.hpp>

#include <gnuradio/io_signature.h>

#include <gnuradio/blocks/short_to_float.h>
#include <gnuradio/blocks/float_to_complex.h>

#include <volk/volk.h>
#include "yunsdr_source_impl.h"

using namespace boost::assign;
using namespace gr::blocks;

#define BUF_LEN  (65536)
#define BUF_NUM   1500

#define BYTES_PER_SAMPLE  4

namespace gr {
    namespace yunsdr {

        static inline void
            yunsdr_32u_deinterleave_32u_x2_generic(unsigned int* Buffer0, 
                    unsigned int* Buffer1, const unsigned int* Vector,
                    unsigned int num_points)
            {
                const unsigned int* VectorPtr = Vector;
                unsigned int* BufferPtr0 = Buffer0;
                unsigned int* BufferPtr1 = Buffer1;
                unsigned int number;
                for(number = 0; number < num_points; number++) {
                    *BufferPtr0++ = *VectorPtr++;
                    *BufferPtr1++ = *VectorPtr++;
                }
            }

        yunsdr_source::sptr
            yunsdr_source::make(const std::string &uri, unsigned long long frequency,
                    unsigned long samplerate,
                    unsigned long bandwidth,
                    bool rx1_en, bool rx2_en,
                    const char *gain1, double gain1_value,
                    const char *gain2, double gain2_value,
                    const std::string &ref_clock,
                    const std::string &vco,
                    unsigned int buffer_size,
                    const char *rf_port_select, const char *filter,
                    bool auto_filter)
            {
                return gnuradio::get_initial_sptr
                    (new yunsdr_source_impl(uri, frequency, 
                                            samplerate, bandwidth,
                                            rx1_en, rx2_en,
                                            gain1, gain1_value,
                                            gain2, gain2_value, 
                                            ref_clock, vco,
                                            buffer_size,
                                            rf_port_select, filter,
                                            auto_filter));
            }

        yunsdr_source_impl::yunsdr_source_impl(const std::string &uri, unsigned long long frequency, 
                unsigned long samplerate,
                unsigned long bandwidth,
                bool rx1_en, bool rx2_en,
                const char *gain1, double gain1_value,
                const char *gain2, double gain2_value,
                const std::string &ref_clock,
                const std::string &vco,
                unsigned int buffer_size,
                const char *rf_port_select, const char *filter,
                bool auto_filter)
            : gr::sync_block("yunsdr_source",
                    gr::io_signature::make(0, 0, 0),
                    gr::io_signature::make(
                        (int) rx1_en + (int) rx2_en,
                        (int) rx1_en + (int) rx2_en,
                        sizeof(gr_complex))),
            _dev(NULL)

        {
            int ret;

            _ipaddr = uri;
            std::cerr << "Using YunSDR URI = " << _ipaddr << std::endl;

            _dev = yunsdr_open_device(_ipaddr.c_str());
            if(_dev == NULL)
                throw std::runtime_error("Failed to open YunSDR device");

            std::cerr << "Using YunSDR with firmware [SDR]"<< std::endl;

            if (ref_clock == "external")
                ret = yunsdr_set_ref_clock(_dev, EXTERNAL_REFERENCE);
            else
                ret = yunsdr_set_ref_clock(_dev, INTERNAL_REFERENCE);

            if ( ret < 0 )
                std::cerr << "Failed to configure YunSDR's reference clock to : " << ref_clock << std::endl;
            else
                std::cerr << "Configure YunSDR's ref_clock to " << ref_clock << std::endl;

            uint32_t vco_param;
            if (vco == "adf4001") {
                ret = yunsdr_set_vco_select(_dev, ADF4001);
                vco_param = 10<<16|26;
                ret = yunsdr_set_adf4001(_dev, vco_param);
            } else {
                ret = yunsdr_set_vco_select(_dev, AUXDAC1);
                vco_param = 1450;
                ret = yunsdr_set_auxdac1(_dev, vco_param);
            }
            if ( ret < 0 )
                std::cerr << "Failed to configure YunSDR's VCO to : " << vco << std::endl;
            else
                std::cerr << "Configure YunSDR's VCO to " << vco << " 1450 mV" << std::endl;

            if(!strcmp(rf_port_select, "TRX")) {
                ret = yunsdr_set_trx_select(_dev, RX);
                if ( ret < 0 )
                    std::cerr << "Failed to configure YunSDR Antenna to RX " << std::endl;
                else
                    std::cerr << "Configure YunSDR Antenna to TRX A/B" << std::endl;

                ret = yunsdr_set_duplex_select(_dev, TDD);
                if ( ret < 0 )
                    std::cerr << "Failed to configure YunSDR duplex mode to : TDD" << std::endl;
                else
                    std::cerr << "Configure YunSDR duplex mode to TDD" << std::endl;
            } else {
                ret = yunsdr_set_trx_select(_dev, TX);
                if ( ret < 0 )
                    std::cerr << "Failed to configure YunSDR Antenna to RX: " << std::endl;
                else
                    std::cerr << "Configure YunSDR Antenna to RX" << std::endl;

                ret = yunsdr_set_duplex_select(_dev, FDD);
                if ( ret < 0 )
                    std::cerr << "Failed to configure YunSDR duplex mode to : FDD" << std::endl;
                else
                    std::cerr << "Configure YunSDR duplex mode to FDD" << std::endl;
            }

            set_params(frequency, samplerate, bandwidth,
                    gain1, gain1_value,
                    gain2, gain2_value,
                    rf_port_select, filter, auto_filter);

            if(rx1_en && rx2_en)
                rx_channel = RX_DUALCHANNEL;
            else if(rx1_en && !rx2_en)
                rx_channel = RX1_CHANNEL;
            else
                rx_channel = RX2_CHANNEL;

            std::cerr << "Using YunSDR  " << 
                (rx_channel == RX_DUALCHANNEL?"RX_DUALCHANNEL":
                 ((rx_channel == RX1_CHANNEL)?"RX1_CHANNEL":"RX2_CHANNEL")) << std::endl; 

            _buf_num = BUF_NUM;
            _buf_len = BUF_LEN*(rx_channel == RX_DUALCHANNEL?2:1);
            _samp_avail = _buf_len / BYTES_PER_SAMPLE;

            rx_buf = (short *) malloc(_buf_len);
            cb_init(&_cbuf, _buf_num, _buf_len);

            set_min_output_buffer(0, _buf_len * BYTES_PER_SAMPLE);
            if(rx2_en) {
                set_min_output_buffer(1, _buf_len * BYTES_PER_SAMPLE);
                set_min_noutput_items(_samp_avail / 2);
            } else
                set_min_noutput_items(_samp_avail);

        }

        /*
         * Our virtual destructor.
         */
        yunsdr_source_impl::~yunsdr_source_impl ()
        {
            int ret;
            if (_dev) {
                ret = yunsdr_close_device( _dev );
                if ( ret < 0 )
                {
                    std::cerr << "Failed to close YunSDR" << std::endl;
                }
                _dev = NULL;
            }

            cb_free( &_cbuf );
        }

        void yunsdr_source_impl::set_params(unsigned long long frequency,
                unsigned long samplerate, unsigned long bandwidth,
                const char *gain1, double gain1_value,
                const char *gain2, double gain2_value,
                const char *port_select, const char *filter,
                bool auto_filter)
        {

            if (filter && filter[0])
                auto_filter = false;

            yunsdr_set_rx_lo_freq(_dev, frequency);
            yunsdr_set_rx_sampling_freq(_dev, samplerate);

            if(!strcmp(gain1, "fast_attack"))
                yunsdr_set_rx_gain_control_mode(_dev, RX1_CHANNEL, RF_GAIN_FASTATTACK_AGC);
            else if(!strcmp(gain1, "slow_attack"))
                yunsdr_set_rx_gain_control_mode(_dev, RX1_CHANNEL, RF_GAIN_SLOWATTACK_AGC);
            else {
                yunsdr_set_rx_gain_control_mode(_dev, RX1_CHANNEL, RF_GAIN_MGC);
                yunsdr_set_rx_rf_gain(_dev, RX1_CHANNEL, gain1_value);
            }

            if(!strcmp(gain2, "fast_attack"))
                yunsdr_set_rx_gain_control_mode(_dev, RX2_CHANNEL, RF_GAIN_FASTATTACK_AGC);
            else if(!strcmp(gain2, "slow_attack"))
                yunsdr_set_rx_gain_control_mode(_dev, RX2_CHANNEL, RF_GAIN_SLOWATTACK_AGC);
            else {
                yunsdr_set_rx_gain_control_mode(_dev, RX2_CHANNEL, RF_GAIN_MGC);
                yunsdr_set_rx_rf_gain(_dev, RX2_CHANNEL, gain2_value);
            }

            yunsdr_set_rx_rf_bandwidth(_dev, bandwidth);
#if 0
            if (auto_filter) {
                int ret = ad9361_set_bb_rate(phy, samplerate);
                if (ret) {
                    throw std::runtime_error("Unable to set BB rate");
                }
            } else if (filter && filter[0]) {
                std::string filt(filter);
                if (!load_fir_filter(filt, phy))
                    throw std::runtime_error("Unable to load filter file");
            }
#endif
        }

        void yunsdr_source_impl::_yunsdr_wait(yunsdr_source_impl *obj)
        {
            obj->yunsdr_wait();
        }

        void yunsdr_source_impl::yunsdr_wait()
        {
            int32_t ret;
            while (running) {
                uint64_t timestamp = 0;
                ret = yunsdr_read_samples(_dev, (void **)&rx_buf, _buf_len, &timestamp, 0);
                if(ret < 0)
                    throw std::runtime_error("Failed to read samples from YunSDR");
                {
                    boost::mutex::scoped_lock lock( _buf_mutex );

                    if ( ! cb_has_room(&_cbuf) )
                        std::cerr << "O" << std::flush;
                    else {
                        if ( ! cb_push_back( &_cbuf, rx_buf ) )
                            std::cerr << "O" << std::flush;
                    }
                }
                _buf_cond.notify_one();
            }

            return;

        }

        int yunsdr_source_impl::work( int noutput_items,
                gr_vector_const_void_star &input_items,
                gr_vector_void_star &output_items )
        {
            const float scaling = 32768.0f;

            if ( ! running )
                return WORK_DONE;

            if(rx_channel == RX_DUALCHANNEL) {
                gr_complex *out0 = static_cast<gr_complex *>(output_items[0]);
                gr_complex *out1 = static_cast<gr_complex *>(output_items[1]);
                unsigned int buf[_buf_len];

                {
                    boost::mutex::scoped_lock lock( _buf_mutex );
                    while ( ! cb_pop_front( &_cbuf, buf ) ) {
                        //std::cerr << "U" << std::flush;
                        _buf_cond.wait( lock );
                    } 
                }
                unsigned int rx1[_samp_avail/2];
                unsigned int rx2[_samp_avail/2];

                yunsdr_32u_deinterleave_32u_x2_generic(rx1, rx2, buf, _samp_avail/2);
                volk_16i_s32f_convert_32f((float*)out0, (short *)rx1, scaling, _samp_avail);
                volk_16i_s32f_convert_32f((float*)out1, (short *)rx2, scaling, _samp_avail);

                return _samp_avail/2;

            } else {
                gr_complex *out = static_cast<gr_complex *>(output_items[0]);
                short buf[_buf_len];

                {
                    boost::mutex::scoped_lock lock( _buf_mutex );
                    while ( ! cb_pop_front( &_cbuf, buf ) ) {
                        //std::cerr << "U" << std::flush;
                        _buf_cond.wait( lock );
                    }
                }

                volk_16i_s32f_convert_32f((float*)out, buf, scaling, 2*_samp_avail);
                return _samp_avail;
            }
            //return noutput_items;
        }
        bool yunsdr_source_impl::start()
        {   
            int ret;

            ret = yunsdr_enable_rx(_dev, _buf_len / (rx_channel == RX_DUALCHANNEL?8:4), 
                    rx_channel, START_RX_NORMAL, 0);
            if (ret < 0)
                throw std::runtime_error("Failed to start RX streaming");

            running = true;
            _thread = gr::thread::thread(_yunsdr_wait, this);

            return true;
        }

        bool yunsdr_source_impl::stop()
        {
            running = false;
            _thread.join();
            return true;
        }

    }
}

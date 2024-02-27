/*
 * Copyright (C) 2023 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2023 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-autogain
 * Created on: 21 сен 2023 г.
 *
 * lsp-plugins-autogain is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins-autogain is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins-autogain. If not, see <https://www.gnu.org/licenses/>.
 */

#include <lsp-plug.in/common/alloc.h>
#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/dsp/dsp.h>
#include <lsp-plug.in/dsp-units/units.h>
#include <lsp-plug.in/plug-fw/meta/func.h>
#include <lsp-plug.in/shared/id_colors.h>

#include <private/plugins/autogain.h>

/* The size of temporary buffer for audio processing */
#define BUFFER_SIZE         0x400U

namespace lsp
{
    static plug::IPort *TRACE_PORT(plug::IPort *p)
    {
        lsp_trace("  port id=%s", (p)->metadata()->id);
        return p;
    }

    namespace plugins
    {
        /* Gain numberators multiplied by 10 */
        static const uint8_t gain_numerators[] = { 1, 5, 10, 30, 60, 90, 100, 120, 150, 180, 200, 210, 240 };

        //---------------------------------------------------------------------
        // Plugin factory
        static const meta::plugin_t *plugins[] =
        {
            &meta::autogain_mono,
            &meta::autogain_stereo,
            &meta::sc_autogain_mono,
            &meta::sc_autogain_stereo
        };

        static plug::Module *plugin_factory(const meta::plugin_t *meta)
        {
            return new autogain(meta);
        }

        static plug::Factory factory(plugin_factory, plugins, 4);

        //---------------------------------------------------------------------
        // Implementation
        autogain::autogain(const meta::plugin_t *meta):
            Module(meta)
        {
            // Compute the number of audio channels by the number of inputs
            nChannels       = 0;
            enScMode        = meta::autogain::SCMODE_INTERNAL;
            bSidechain      = false;
            for (const meta::port_t *p = meta->ports; p->id != NULL; ++p)
                if (meta::is_audio_out_port(p))
                    ++nChannels;

            if ((!strcmp(meta->uid, meta::sc_autogain_mono.uid)) ||
                (!strcmp(meta->uid, meta::sc_autogain_stereo.uid)))
                bSidechain      = true;

            // Initialize other parameters
            vChannels       = NULL;

            fLInGain        = 0.0f;
            fSInGain        = 0.0f;
            fLOutGain       = 0.0f;
            fSOutGain       = 0.0f;
            fLScGain        = 0.0f;
            fSScGain        = 0.0f;
            fGain           = 0.0f;
            fOldLevel       = dspu::db_to_gain(meta::autogain::LEVEL_DFL);
            fLevel          = dspu::db_to_gain(meta::autogain::LEVEL_DFL);
            fOldPreamp      = 0.0f;
            fPreamp         = 1.0f;

            vLBuffer        = NULL;
            vSBuffer        = NULL;
            vGainBuffer     = NULL;
            vTimePoints     = NULL;

            pBypass         = NULL;
            pScMode         = NULL;
            pScPreamp       = NULL;
            pLookahead      = NULL;
            pLPeriod        = NULL;
            pSPeriod        = NULL;
            pWeighting      = NULL;
            pLevel          = NULL;
            pDeviation      = NULL;
            pSilence        = NULL;
            pAmpOn          = NULL;
            pAmpGain        = NULL;
            pQAmp           = NULL;

            for (size_t i=0; i<GCT_TOTAL; ++i)
            {
                gcontrol_t *gc  = &vGainCtl[i];
                gc->pValue      = NULL;
                gc->pPeroid     = NULL;
            }

            pLInGain        = NULL;
            pSInGain        = NULL;
            pLOutGain       = NULL;
            pSOutGain       = NULL;
            pLScGain        = NULL;
            pSScGain        = NULL;
            pGain           = NULL;
            pLInGraph       = NULL;
            pSInGraph       = NULL;
            pLOutGraph      = NULL;
            pSOutGraph      = NULL;
            pLScGraph       = NULL;
            pSScGraph       = NULL;
            pGainGraph      = NULL;

            pIDisplay       = NULL;

            pData           = NULL;
        }

        autogain::~autogain()
        {
            do_destroy();
        }

        void autogain::init(plug::IWrapper *wrapper, plug::IPort **ports)
        {
            // Call parent class for initialization
            status_t res;
            Module::init(wrapper, ports);

            // Estimate the number of bytes to allocate
            size_t szof_channels    = align_size(sizeof(channel_t) * nChannels, OPTIMAL_ALIGN);
            size_t szof_buffer      = BUFFER_SIZE * sizeof(float);
            size_t szof_graph       = meta::autogain::MESH_POINTS * sizeof(float);
            size_t alloc            =
                szof_channels +     // vChannels
                szof_buffer +       // vLBuffer
                szof_buffer +       // vSBuffer
                szof_buffer +       // vGainBuffer
                szof_graph +        // vTimePoints
                nChannels * (
                    szof_buffer     // vBuffer
                );

            // Allocate memory-aligned data
            uint8_t *ptr            = alloc_aligned<uint8_t>(pData, alloc, OPTIMAL_ALIGN);
            if (ptr == NULL)
                return;

            if ((res = sLInMeter.init(nChannels, meta::autogain::LONG_PERIOD_MAX)) != STATUS_OK)
                return;
            if ((res = sSInMeter.init(nChannels, meta::autogain::SHORT_PERIOD_MAX)) != STATUS_OK)
                return;
            if ((res = sLOutMeter.init(nChannels, meta::autogain::LONG_PERIOD_MAX)) != STATUS_OK)
                return;
            if ((res = sSOutMeter.init(nChannels, meta::autogain::SHORT_PERIOD_MAX)) != STATUS_OK)
                return;
            if ((res = sLScMeter.init(nChannels, meta::autogain::LONG_PERIOD_MAX)) != STATUS_OK)
                return;
            if ((res = sSScMeter.init(nChannels, meta::autogain::SHORT_PERIOD_MAX)) != STATUS_OK)
                return;
            if ((res = sAutoGain.init()) != STATUS_OK)
                return;

            // Initialize pointers to channels and temporary buffer
            vChannels               = reinterpret_cast<channel_t *>(ptr);
            ptr                    += szof_channels;
            vLBuffer                = reinterpret_cast<float *>(ptr);
            ptr                    += szof_buffer;
            vSBuffer                = reinterpret_cast<float *>(ptr);
            ptr                    += szof_buffer;
            vGainBuffer             = reinterpret_cast<float *>(ptr);
            ptr                    += szof_buffer;
            vTimePoints             = reinterpret_cast<float *>(ptr);
            ptr                    += szof_graph;

            for (size_t i=0; i < nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

                c->sBypass.construct();
                c->sDelay.construct();

                c->vIn                  = NULL;
                c->vScIn                = NULL;
                c->vOut                 = NULL;

                c->vBuffer              = reinterpret_cast<float *>(ptr);
                ptr                    += szof_buffer;

                c->pIn                  = NULL;
                c->pScIn                = NULL;
                c->pOut                 = NULL;
            }

            // Bind ports
            lsp_trace("Binding ports");
            size_t port_id      = 0;

            // Bind input audio ports
            for (size_t i=0; i<nChannels; ++i)
                vChannels[i].pIn    = TRACE_PORT(ports[port_id++]);

            // Bind output audio ports
            for (size_t i=0; i<nChannels; ++i)
                vChannels[i].pOut   = TRACE_PORT(ports[port_id++]);

            // Bind sidechain audio ports
            if (bSidechain)
            {
                for (size_t i=0; i<nChannels; ++i)
                    vChannels[i].pScIn  = TRACE_PORT(ports[port_id++]);
            }

            // Bind bypass
            pBypass             = TRACE_PORT(ports[port_id++]);

            // Bind sidechain ports
            lsp_trace("Binding sidechain controls");
            pScPreamp           = TRACE_PORT(ports[port_id++]);
            pLookahead          = TRACE_PORT(ports[port_id++]);
            if (bSidechain)
            {
                pScMode             = TRACE_PORT(ports[port_id++]);
                TRACE_PORT(ports[port_id++]); // Skip enable sidechain metering port for long period
                TRACE_PORT(ports[port_id++]); // Skip enable sidechain metering port for short period
                pLScGain            = TRACE_PORT(ports[port_id++]);
                pSScGain            = TRACE_PORT(ports[port_id++]);
                pLScGraph           = TRACE_PORT(ports[port_id++]);
                pSScGraph           = TRACE_PORT(ports[port_id++]);
            }

            // Bind common ports
            lsp_trace("Binding common controls");
            pLPeriod            = TRACE_PORT(ports[port_id++]);
            pSPeriod            = TRACE_PORT(ports[port_id++]);
            pWeighting          = TRACE_PORT(ports[port_id++]);
            pLevel              = TRACE_PORT(ports[port_id++]);
            pDeviation          = TRACE_PORT(ports[port_id++]);
            pSilence            = TRACE_PORT(ports[port_id++]);
            pAmpOn              = TRACE_PORT(ports[port_id++]);
            pAmpGain            = TRACE_PORT(ports[port_id++]);
            pQAmp               = TRACE_PORT(ports[port_id++]);

            lsp_trace("Binding gain controls");
            for (size_t i=0; i<GCT_TOTAL; ++i)
            {
                gcontrol_t *gc  = &vGainCtl[i];
                gc->pValue      = TRACE_PORT(ports[port_id++]);
                gc->pPeroid     = TRACE_PORT(ports[port_id++]);
            }

            lsp_trace("Binding metering controls");
            TRACE_PORT(ports[port_id++]); // Skip enable input gain metering port for long period
            TRACE_PORT(ports[port_id++]); // Skip enable input gain metering port for short period
            TRACE_PORT(ports[port_id++]); // Skip enable output gain metering port for long period
            TRACE_PORT(ports[port_id++]); // Skip enable output gain metering port for short period
            TRACE_PORT(ports[port_id++]); // Skip enable gain correction metering
            pLInGain            = TRACE_PORT(ports[port_id++]);
            pSInGain            = TRACE_PORT(ports[port_id++]);
            pLOutGain           = TRACE_PORT(ports[port_id++]);
            pSOutGain           = TRACE_PORT(ports[port_id++]);
            pGain               = TRACE_PORT(ports[port_id++]);
            pLInGraph           = TRACE_PORT(ports[port_id++]);
            pSInGraph           = TRACE_PORT(ports[port_id++]);
            pLOutGraph          = TRACE_PORT(ports[port_id++]);
            pSOutGraph          = TRACE_PORT(ports[port_id++]);
            pGainGraph          = TRACE_PORT(ports[port_id++]);

            // Fill values
            float k     = meta::autogain::MESH_TIME / (meta::autogain::MESH_POINTS - 1);
            for (size_t i=0; i<meta::autogain::MESH_POINTS; ++i)
                vTimePoints[i] =  meta::autogain::MESH_TIME - k*i;
        }

        void autogain::destroy()
        {
            Module::destroy();
            do_destroy();
        }

        void autogain::do_destroy()
        {
            sLInGraph.destroy();
            sSInGraph.destroy();
            sLOutGraph.destroy();
            sSOutGraph.destroy();
            sLScGraph.destroy();
            sSScGraph.destroy();
            sGainGraph.destroy();

            sLInMeter.destroy();
            sSInMeter.destroy();
            sLOutMeter.destroy();
            sSOutMeter.destroy();
            sLScMeter.destroy();
            sSScMeter.destroy();

            sAutoGain.destroy();

            // Destroy channels
            if (vChannels != NULL)
            {
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c    = &vChannels[i];
                    c->sDelay.destroy();
                    c->sBypass.destroy();
                }
                vChannels   = NULL;
            }

            // Destroy inline display
            if (pIDisplay != NULL)
            {
                pIDisplay->destroy();
                pIDisplay   = NULL;
            }

            // Free previously allocated data chunk
            if (pData != NULL)
            {
                free_aligned(pData);
                pData       = NULL;
            }
        }

        void autogain::update_sample_rate(long sr)
        {
            size_t samples_per_dot  = dspu::seconds_to_samples(
                sr, meta::autogain::MESH_TIME / meta::autogain::MESH_POINTS);

            sLInGraph.init(meta::autogain::MESH_POINTS, samples_per_dot);
            sSInGraph.init(meta::autogain::MESH_POINTS, samples_per_dot);
            sLOutGraph.init(meta::autogain::MESH_POINTS, samples_per_dot);
            sSOutGraph.init(meta::autogain::MESH_POINTS, samples_per_dot);
            sLScGraph.init(meta::autogain::MESH_POINTS, samples_per_dot);
            sSScGraph.init(meta::autogain::MESH_POINTS, samples_per_dot);
            sGainGraph.init(meta::autogain::MESH_POINTS, samples_per_dot);

            sLInMeter.set_sample_rate(sr);
            sSInMeter.set_sample_rate(sr);
            sLOutMeter.set_sample_rate(sr);
            sSOutMeter.set_sample_rate(sr);
            sLScMeter.set_sample_rate(sr);
            sSScMeter.set_sample_rate(sr);

            sAutoGain.set_sample_rate(sr);

            size_t max_delay = dspu::millis_to_samples(sr, meta::autogain::SC_LOOKAHEAD_MAX);

            // Update sample rate for the bypass processors
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];
                c->sDelay.init(max_delay);
                c->sBypass.init(sr);
            }
        }

        dspu::bs::weighting_t autogain::decode_weighting(size_t weighting)
        {
            switch (weighting)
            {
                case meta::autogain::WEIGHT_A:  return dspu::bs::WEIGHT_A;
                case meta::autogain::WEIGHT_B:  return dspu::bs::WEIGHT_B;
                case meta::autogain::WEIGHT_C:  return dspu::bs::WEIGHT_C;
                case meta::autogain::WEIGHT_D:  return dspu::bs::WEIGHT_D;
                case meta::autogain::WEIGHT_K:  return dspu::bs::WEIGHT_K;
                case meta::autogain::WEIGHT_NONE:
                default:
                    break;
            }

            return dspu::bs::WEIGHT_NONE;
        }

        float autogain::calc_gain_speed(gcontrol_type_t type)
        {
            gcontrol_t *gc  = &vGainCtl[type];

            size_t numerator= size_t(gc->pValue->value());
            size_t idx      = lsp_limit(numerator, 0U, (sizeof(gain_numerators)/sizeof(gain_numerators[0])) - 1);
            float fnum      = gain_numerators[idx] * 0.1f;
            float time      = gc->pPeroid->value() * 0.001f;

            return fnum / time ; // Return value in dB/s
        }

        void autogain::update_settings()
        {
            bool bypass                     = pBypass->value() >= 0.5f;
            dspu::bs::weighting_t weight    = decode_weighting(pWeighting->value());

            // Update level
            fLevel                          = dspu::db_to_gain(pLevel->value());
            enScMode                        = (pScMode != NULL) ? size_t(pScMode->value()) : meta::autogain::SCMODE_DFL;
            fPreamp                         = dspu::db_to_gain(pScPreamp->value());
            size_t lookahead                = dspu::millis_to_samples(fSampleRate, pLookahead->value());

            // Configure autogain
            sAutoGain.set_deviation(
                dspu::db_to_gain(pDeviation->value()));
            sAutoGain.set_long_speed(
                calc_gain_speed(GCT_LONG_GROW),
                calc_gain_speed(GCT_LONG_FALL));
            sAutoGain.set_short_speed(
                calc_gain_speed(GCT_SHORT_GROW),
                calc_gain_speed(GCT_SHORT_FALL));
            sAutoGain.set_silence_threshold(
                dspu::db_to_gain(pSilence->value()));
            sAutoGain.enable_quick_amplifier(pQAmp->value() >= 0.5f);
            sAutoGain.set_max_gain(
                dspu::db_to_gain(pAmpGain->value()),
                pAmpOn->value() >= 0.5f);

            // Set measuring period
            float l_period                  = pLPeriod->value();
            float s_period                  = pSPeriod->value();

            sLInMeter.set_period(l_period);
            sSInMeter.set_period(s_period);
            sLInMeter.set_weighting(weight);
            sSInMeter.set_weighting(weight);

            sLOutMeter.set_period(l_period);
            sSOutMeter.set_period(s_period);
            sLOutMeter.set_weighting(weight);
            sSOutMeter.set_weighting(weight);

            sLScMeter.set_period(l_period);
            sSScMeter.set_period(s_period);
            sLScMeter.set_weighting(weight);
            sSScMeter.set_weighting(weight);

            if (nChannels > 1)
            {
                sLInMeter.set_designation(0, dspu::bs::CHANNEL_LEFT);
                sLInMeter.set_designation(1, dspu::bs::CHANNEL_RIGHT);
                sSInMeter.set_designation(0, dspu::bs::CHANNEL_LEFT);
                sSInMeter.set_designation(1, dspu::bs::CHANNEL_RIGHT);

                sLOutMeter.set_designation(0, dspu::bs::CHANNEL_LEFT);
                sLOutMeter.set_designation(1, dspu::bs::CHANNEL_RIGHT);
                sSOutMeter.set_designation(0, dspu::bs::CHANNEL_LEFT);
                sSOutMeter.set_designation(1, dspu::bs::CHANNEL_RIGHT);

                sLScMeter.set_designation(0, dspu::bs::CHANNEL_LEFT);
                sLScMeter.set_designation(1, dspu::bs::CHANNEL_RIGHT);
                sSScMeter.set_designation(0, dspu::bs::CHANNEL_LEFT);
                sSScMeter.set_designation(1, dspu::bs::CHANNEL_RIGHT);
            }
            else
            {
                sLInMeter.set_designation(0, dspu::bs::CHANNEL_CENTER);
                sSInMeter.set_designation(0, dspu::bs::CHANNEL_CENTER);

                sLOutMeter.set_designation(0, dspu::bs::CHANNEL_CENTER);
                sSOutMeter.set_designation(0, dspu::bs::CHANNEL_CENTER);

                sLScMeter.set_designation(0, dspu::bs::CHANNEL_CENTER);
                sSScMeter.set_designation(0, dspu::bs::CHANNEL_CENTER);
            }

            for (size_t i=0; i<nChannels; ++i)
            {
                sLInMeter.set_link(i, 1.0f);
                sLInMeter.set_active(i, true);
                sSInMeter.set_link(i, 1.0f);
                sSInMeter.set_active(i, true);

                sLOutMeter.set_link(i, 1.0f);
                sLOutMeter.set_active(i, true);
                sSOutMeter.set_link(i, 1.0f);
                sSOutMeter.set_active(i, true);

                sLScMeter.set_link(i, 1.0f);
                sLScMeter.set_active(i, true);
                sSScMeter.set_link(i, 1.0f);
                sSScMeter.set_active(i, true);
            }

            // Update bypass
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];
                c->sDelay.set_delay(lookahead);
                c->sBypass.set_bypass(bypass);
            }

            // Report latency
            set_latency(lookahead);
        }

        void autogain::process(size_t samples)
        {
            bind_audio_ports();
            clean_meters();

            for (size_t offset=0; offset < samples; )
            {
                size_t to_do    = lsp_min(samples - offset, BUFFER_SIZE);

                measure_input_loudness(to_do);
                compute_gain_correction(to_do);
                apply_gain_correction(to_do);
                update_audio_buffers(to_do);

                offset         += to_do;
            }

            output_meters();
            output_mesh_data();

            // Request for redraw
            if (pWrapper != NULL)
                pWrapper->query_display_draw();
        }

        void autogain::bind_audio_ports()
        {
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];

                c->vIn          = c->pIn->buffer<float>();
                c->vScIn        = (c->pScIn != NULL) ? c->pScIn->buffer<float>() : c->vIn;
                c->vOut         = c->pOut->buffer<float>();
            }
        }

        void autogain::clean_meters()
        {
            fLInGain        = 0.0f;
            fSInGain        = 0.0f;
            fLOutGain       = 0.0f;
            fSOutGain       = 0.0f;
            fLScGain        = 0.0f;
            fSScGain        = 0.0f;
            fGain           = 0.0f;
        }

        void autogain::measure_input_loudness(size_t samples)
        {
            // Bind channels for analysis
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];

                sLInMeter.bind(i, NULL, c->vIn, 0);
                sSInMeter.bind(i, NULL, c->vIn, 0);

                // Process sidechain signal
                switch (enScMode)
                {
                    case meta::autogain::SCMODE_CONTROL:
                    case meta::autogain::SCMODE_MATCH:
                        dsp::lramp2(c->vBuffer, c->vScIn, fOldPreamp, fPreamp, samples);
                        break;

                    case meta::autogain::SCMODE_INTERNAL:
                    default:
                        dsp::lramp2(c->vBuffer, c->vIn, fOldPreamp, fPreamp, samples);
                        break;
                }

                // Bind sidechain meters
                if (bSidechain)
                {
                    sLScMeter.bind(i, NULL, c->vBuffer, 0);
                    sSScMeter.bind(i, NULL, c->vBuffer, 0);
                }
                else
                {
                    sLInMeter.bind(i, NULL, c->vBuffer, 0);
                    sSInMeter.bind(i, NULL, c->vBuffer, 0);
                }
            }
            fOldPreamp  = fPreamp;

            // Depending on the operating mode, we need to change the order of processing input and sidechain signals
            switch (enScMode)
            {
                case meta::autogain::SCMODE_MATCH:
                    // First process sidechain signal
                    if (bSidechain)
                    {
                        sLScMeter.process(vLBuffer, samples, dspu::bs::DBFS_TO_LUFS_SHIFT_GAIN);
                        fLScGain    = lsp_max(fLInGain, dsp::max(vLBuffer, samples));
                        sLScGraph.process(vLBuffer, samples);

                        sSScMeter.process(vSBuffer, samples, dspu::bs::DBFS_TO_LUFS_SHIFT_GAIN);
                        fSScGain    = lsp_max(fSInGain, dsp::max(vSBuffer, samples));
                        sSScGraph.process(vSBuffer, samples);

                        // Limit the long sidechain signal and put to the buffer
                        dsp::limit2(
                            vGainBuffer,
                            vLBuffer,
                            meta::autogain::LEVEL_GAIN_MIN,
                            meta::autogain::LEVEL_GAIN_MAX,
                            samples);
                    }

                    // Then process input signal as usual
                    sLInMeter.process(vLBuffer, samples, dspu::bs::DBFS_TO_LUFS_SHIFT_GAIN);
                    fLInGain    = lsp_max(fLInGain, dsp::max(vLBuffer, samples));
                    sLInGraph.process(vLBuffer, samples);

                    sSInMeter.process(vSBuffer, samples, dspu::bs::DBFS_TO_LUFS_SHIFT_GAIN);
                    fSInGain    = lsp_max(fSInGain, dsp::max(vSBuffer, samples));
                    sSInGraph.process(vSBuffer, samples);

                    break;

                case meta::autogain::SCMODE_CONTROL:
                case meta::autogain::SCMODE_INTERNAL:
                default:
                    // Process the loudnes of input signal
                    sLInMeter.process(vLBuffer, samples, dspu::bs::DBFS_TO_LUFS_SHIFT_GAIN);
                    fLInGain    = lsp_max(fLInGain, dsp::max(vLBuffer, samples));
                    sLInGraph.process(vLBuffer, samples);

                    sSInMeter.process(vSBuffer, samples, dspu::bs::DBFS_TO_LUFS_SHIFT_GAIN);
                    fSInGain    = lsp_max(fSInGain, dsp::max(vSBuffer, samples));
                    sSInGraph.process(vSBuffer, samples);

                    // Process the loudness of sidechain signal
                    if (bSidechain)
                    {
                        sLScMeter.process(vLBuffer, samples, dspu::bs::DBFS_TO_LUFS_SHIFT_GAIN);
                        fLScGain    = lsp_max(fLInGain, dsp::max(vLBuffer, samples));
                        sLScGraph.process(vLBuffer, samples);

                        sSScMeter.process(vSBuffer, samples, dspu::bs::DBFS_TO_LUFS_SHIFT_GAIN);
                        fSScGain    = lsp_max(fSInGain, dsp::max(vSBuffer, samples));
                        sSScGraph.process(vSBuffer, samples);
                    }
                    break;
            }
        }

        void autogain::compute_gain_correction(size_t samples)
        {
            switch (enScMode)
            {
                case meta::autogain::SCMODE_MATCH:
                    // In 'Match' mode the sidechain channel defines the desired level of loudness.
                    // The actual sidechain level is already stored in the vGainBuffer.
                    sAutoGain.process(vGainBuffer, vLBuffer, vSBuffer, vGainBuffer, samples);
                    break;

                case meta::autogain::SCMODE_CONTROL:
                case meta::autogain::SCMODE_INTERNAL:
                default:
                    // Process autogain
                    if (fOldLevel != fLevel)
                    {
                        dsp::lramp_set1(vGainBuffer, fOldLevel, fLevel, samples);
                        sAutoGain.process(vGainBuffer, vLBuffer, vSBuffer, vGainBuffer, samples);
                    }
                    else
                        sAutoGain.process(vGainBuffer, vLBuffer, vSBuffer, fLevel, samples);
                    break;
            }
            fOldLevel   = fLevel;

            // Collect autogain metering
            fGain       = lsp_max(fGain, dsp::max(vGainBuffer, samples));
            sGainGraph.process(vGainBuffer, samples);
        }

        void autogain::apply_gain_correction(size_t samples)
        {
            // Apply gain correction to each channel
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];

                c->sDelay.process(c->vBuffer, c->vBuffer, samples);     // Apply lookahead to the delay
                dsp::mul3(c->vBuffer, c->vIn, vGainBuffer, samples);    // Apply VCA control

                sLOutMeter.bind(i, NULL, c->vBuffer, 0);
                sSOutMeter.bind(i, NULL, c->vBuffer, 0);
            }

            sLOutMeter.process(vLBuffer, samples, dspu::bs::DBFS_TO_LUFS_SHIFT_GAIN);
            fLOutGain    = lsp_max(fLOutGain, dsp::max(vLBuffer, samples));
            sLOutGraph.process(vLBuffer, samples);

            sSOutMeter.process(vSBuffer, samples, dspu::bs::DBFS_TO_LUFS_SHIFT_GAIN);
            fSOutGain    = lsp_max(fSOutGain, dsp::max(vSBuffer, samples));
            sSOutGraph.process(vSBuffer, samples);
        }

        void autogain::update_audio_buffers(size_t samples)
        {
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];

                // Apply bypass
                c->sBypass.process(c->vOut, c->vIn, c->vBuffer, samples);

                // Move pointers
                c->vIn         += samples;
                c->vScIn       += samples;
                c->vOut        += samples;
            }
        }

        void autogain::output_meters()
        {
            pLInGain->set_value(fLInGain);
            pSInGain->set_value(fSInGain);
            pLOutGain->set_value(fLOutGain);
            pSOutGain->set_value(fSOutGain);
            if (bSidechain)
            {
                pLScGain->set_value(fLScGain);
                pSScGain->set_value(fSScGain);
            }
            pGain->set_value(fGain);
        }

        void autogain::output_mesh_data()
        {
            plug::mesh_t *mesh;

            // Sync input gain meshes
            mesh    = pLInGraph->buffer<plug::mesh_t>();
            if ((mesh != NULL) && (mesh->isEmpty()))
            {
                dsp::copy(mesh->pvData[0], vTimePoints, meta::autogain::MESH_POINTS);
                dsp::copy(mesh->pvData[1], sLInGraph.data(), meta::autogain::MESH_POINTS);
                mesh->data(2, meta::autogain::MESH_POINTS);
            }

            mesh    = pSInGraph->buffer<plug::mesh_t>();
            if ((mesh != NULL) && (mesh->isEmpty()))
            {
                float *x = mesh->pvData[0];
                float *y = mesh->pvData[1];

                dsp::copy(&x[1], vTimePoints, meta::autogain::MESH_POINTS);
                dsp::copy(&y[1], sSInGraph.data(), meta::autogain::MESH_POINTS);


                x[0] = x[1];
                y[0] = 0.0f;

                x += meta::autogain::MESH_POINTS + 1;
                y += meta::autogain::MESH_POINTS + 1;
                x[0] = x[-1];
                y[0] = 0.0f;

                mesh->data(2, meta::autogain::MESH_POINTS + 2);
            }

            mesh    = pLOutGraph->buffer<plug::mesh_t>();
            if ((mesh != NULL) && (mesh->isEmpty()))
            {
                dsp::copy(mesh->pvData[0], vTimePoints, meta::autogain::MESH_POINTS);
                dsp::copy(mesh->pvData[1], sLOutGraph.data(), meta::autogain::MESH_POINTS);
                mesh->data(2, meta::autogain::MESH_POINTS);
            }

            mesh    = pSOutGraph->buffer<plug::mesh_t>();
            if ((mesh != NULL) && (mesh->isEmpty()))
            {
                float *x = mesh->pvData[0];
                float *y = mesh->pvData[1];

                dsp::copy(&x[1], vTimePoints, meta::autogain::MESH_POINTS);
                dsp::copy(&y[1], sSInGraph.data(), meta::autogain::MESH_POINTS);


                x[0] = x[1];
                y[0] = 0.0f;

                x += meta::autogain::MESH_POINTS + 1;
                y += meta::autogain::MESH_POINTS + 1;
                x[0] = x[-1];
                y[0] = 0.0f;

                mesh->data(2, meta::autogain::MESH_POINTS + 2);
            }

            // Output sidechain metering
            if (bSidechain)
            {
                mesh    = pLScGraph->buffer<plug::mesh_t>();
                if ((mesh != NULL) && (mesh->isEmpty()))
                {
                    dsp::copy(mesh->pvData[0], vTimePoints, meta::autogain::MESH_POINTS);
                    dsp::copy(mesh->pvData[1], sLScGraph.data(), meta::autogain::MESH_POINTS);
                    mesh->data(2, meta::autogain::MESH_POINTS);
                }

                mesh    = pSScGraph->buffer<plug::mesh_t>();
                if ((mesh != NULL) && (mesh->isEmpty()))
                {
                    float *x = mesh->pvData[0];
                    float *y = mesh->pvData[1];

                    dsp::copy(&x[1], vTimePoints, meta::autogain::MESH_POINTS);
                    dsp::copy(&y[1], sSInGraph.data(), meta::autogain::MESH_POINTS);


                    x[0] = x[1];
                    y[0] = 0.0f;

                    x += meta::autogain::MESH_POINTS + 1;
                    y += meta::autogain::MESH_POINTS + 1;
                    x[0] = x[-1];
                    y[0] = 0.0f;

                    mesh->data(2, meta::autogain::MESH_POINTS + 2);
                }
            }

            mesh    = pGainGraph->buffer<plug::mesh_t>();
            if ((mesh != NULL) && (mesh->isEmpty()))
            {
                float *x = mesh->pvData[0];
                float *y = mesh->pvData[1];

                dsp::copy(&x[2], vTimePoints, meta::autogain::MESH_POINTS);
                dsp::copy(&y[2], sGainGraph.data(), meta::autogain::MESH_POINTS);

                x[0] = x[2] + 0.5f;
                x[1] = x[0];
                y[0] = 1.0f;
                y[1] = y[2];

                x += meta::autogain::MESH_POINTS + 2;
                y += meta::autogain::MESH_POINTS + 2;
                x[0] = x[-1] - 0.5f;
                y[0] = y[-1];
                x[1] = x[0];
                y[1] = 1.0f;

                mesh->data(2, meta::autogain::MESH_POINTS + 4);
            }
        }

        bool autogain::inline_display(plug::ICanvas *cv, size_t width, size_t height)
        {
            // Check proportions
            if (height > (M_RGOLD_RATIO * width))
                height  = M_RGOLD_RATIO * width;

            // Init canvas
            if (!cv->init(width, height))
                return false;
            width   = cv->width();
            height  = cv->height();

            // Clear background
            bool bypassing = vChannels[0].sBypass.bypassing();
            cv->set_color_rgb((bypassing) ? CV_DISABLED : CV_BACKGROUND);
            cv->paint();

            // Calc axis params
            float zy    = 1.0f/GAIN_AMP_M_84_DB;
            float dx    = -float(width/meta::autogain::MESH_TIME);
            float dy    = height/(logf(GAIN_AMP_M_84_DB)-logf(GAIN_AMP_P_24_DB));

            // Draw axis
            cv->set_line_width(1.0);

            // Draw vertical lines
            cv->set_color_rgb(CV_YELLOW, 0.5f);
            for (float i=1.0; i < (meta::autogain::MESH_TIME - 0.1f); i += 1.0f)
            {
                float ax = width + dx*i;
                cv->line(ax, 0, ax, height);
            }

            // Draw horizontal lines
            cv->set_color_rgb(CV_WHITE, 0.5f);
            for (float i=GAIN_AMP_M_72_DB; i<GAIN_AMP_P_24_DB; i *= GAIN_AMP_P_12_DB)
            {
                float ay = height + dy*(logf(i*zy));
                cv->line(0, ay, width, ay);
            }

            // Allocate buffer: t, gain, x, y
            pIDisplay           = core::IDBuffer::reuse(pIDisplay, 4, width);
            core::IDBuffer *b   = pIDisplay;
            if (b == NULL)
                return false;

            float r             = meta::autogain::MESH_POINTS/float(width);

            // Fill time array
            float *t            = b->v[0];
            for (size_t j=0; j<width; ++j)
                t[j]            = vTimePoints[size_t(r*j)];

            cv->set_line_width(2.0f);

            // Draw gain curve
            {
                // Initialize values
                float *ft       = sGainGraph.data();
                float *g        = b->v[1];
                for (size_t k=0; k<width; ++k)
                    g[k]            = ft[size_t(r*k)];

                // Initialize coords
                dsp::fill(b->v[2], width, width);
                dsp::fill(b->v[3], height, width);
                dsp::fmadd_k3(b->v[2], t, dx, width);
                dsp::axis_apply_log1(b->v[3], g, zy, dy, width);

                // Draw channel
                cv->set_color_rgb((bypassing) ? CV_SILVER : CV_BRIGHT_BLUE);
                cv->draw_lines(b->v[2], b->v[3], width);
            }

            // Draw threshold
            cv->set_color_rgb(CV_MAGENTA, 0.5f);
            cv->set_line_width(1.0);
            {
                float ay = height + dy*(logf(fLevel*zy));
                cv->line(0, ay, width, ay);
            }

            return true;
        }

        void autogain::dump(dspu::IStateDumper *v) const
        {
            plug::Module::dump(v);

            v->write_object("sLInGraph", &sLInGraph);
            v->write_object("sSInGraph", &sSInGraph);
            v->write_object("sLOutGraph", &sLOutGraph);
            v->write_object("sSOutGraph", &sSOutGraph);
            v->write_object("sLScGraph", &sLScGraph);
            v->write_object("sSScGraph", &sSScGraph);
            v->write_object("sGainGraph", &sGainGraph);
            v->write_object("sLInMeter", &sLInMeter);
            v->write_object("sSInMeter", &sSInMeter);
            v->write_object("sLOutMeter", &sLOutMeter);
            v->write_object("sSOutMeter", &sSOutMeter);
            v->write_object("sLScMeter", &sLScMeter);
            v->write_object("sSScMeter", &sSScMeter);
            v->write_object("sAutoGain", &sAutoGain);

            v->write("nChannels", nChannels);
            v->write("enScMode", enScMode);
            v->write("bSidechain", bSidechain);

            v->begin_array("vChannels", vChannels, nChannels);
            {
                for (size_t i=0; i<nChannels; ++i)
                {
                    const channel_t *c  = &vChannels[i];
                    v->begin_object(c, sizeof(channel_t));
                    {
                        v->write_object("sBypass", &c->sBypass);
                        v->write_object("sDelay", &c->sDelay);

                        v->write("vIn", c->vIn);
                        v->write("vScIn", c->vScIn);
                        v->write("vOut", c->vOut);
                        v->write("vBuffer", c->vBuffer);

                        v->write("pIn", c->pIn);
                        v->write("pScIn", c->pScIn);
                        v->write("pOut", c->pOut);
                    }
                    v->end_object();
                }
            }
            v->end_array();

            v->write("fLInGain", fLInGain);
            v->write("fSInGain", fSInGain);
            v->write("fLOutGain", fLOutGain);
            v->write("fSOutGain", fSOutGain);
            v->write("fLScGain", fLScGain);
            v->write("fSScGain", fSScGain);
            v->write("fGain", fGain);
            v->write("fOldLevel", fOldLevel);
            v->write("fLevel", fLevel);
            v->write("fOldPreamp", fOldPreamp);
            v->write("fPreamp", fPreamp);

            v->write("vLBuffer", vLBuffer);
            v->write("vSBuffer", vSBuffer);
            v->write("vGainBuffer", vGainBuffer);
            v->write("vTimePoints", vTimePoints);

            v->write("pBypass", pBypass);
            v->write("pScMode", pScMode);
            v->write("pScPreamp", pScPreamp);
            v->write("pLookahead", pLookahead);
            v->write("pLPeriod", pLPeriod);
            v->write("pSPeriod", pSPeriod);
            v->write("pWeighting", pWeighting);
            v->write("pLevel", pLevel);
            v->write("pDeviation", pDeviation);
            v->write("pSilence", pSilence);
            v->write("pAmpOn", pAmpOn);
            v->write("pAmpGain", pAmpGain);
            v->write("pQAmp",pQAmp );

            v->begin_array("vGainCtl", vGainCtl, GCT_TOTAL);
            for (size_t i=0; i< GCT_TOTAL; ++i)
            {
                const gcontrol_t *gc = &vGainCtl[i];
                v->begin_object(gc, sizeof(gcontrol_t));
                {
                    v->write("pPeroid", gc->pPeroid);
                    v->write("pValue", gc->pValue);
                }
                v->end_object();
            }
            v->end_array();

            v->write("pLInGain", pLInGain);
            v->write("pSInGain", pSInGain);
            v->write("pLOutGain", pLOutGain);
            v->write("pSOutGain", pSOutGain);
            v->write("pLScGain", pLScGain);
            v->write("pSScGain", pSScGain);
            v->write("pGain", pGain);
            v->write("pLInGraph", pLInGraph);
            v->write("pSInGraph", pSInGraph);
            v->write("pLOutGraph", pLOutGraph);
            v->write("pSOutGraph", pSOutGraph);
            v->write("pLScGraph", pLScGraph);
            v->write("pSScGraph", pSScGraph);
            v->write("pGainGraph", pGainGraph);

            v->write("pData", pData);
        }

    } /* namespace plugins */
} /* namespace lsp */



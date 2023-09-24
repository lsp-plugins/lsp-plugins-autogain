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
        //---------------------------------------------------------------------
        // Plugin factory
        static const meta::plugin_t *plugins[] =
        {
            &meta::autogain_mono,
            &meta::autogain_stereo
        };

        static plug::Module *plugin_factory(const meta::plugin_t *meta)
        {
            return new autogain(meta);
        }

        static plug::Factory factory(plugin_factory, plugins, 2);

        //---------------------------------------------------------------------
        // Implementation
        autogain::autogain(const meta::plugin_t *meta):
            Module(meta)
        {
            // Compute the number of audio channels by the number of inputs
            nChannels       = 0;
            for (const meta::port_t *p = meta->ports; p->id != NULL; ++p)
                if (meta::is_audio_in_port(p))
                    ++nChannels;

            // Initialize other parameters
            vChannels       = NULL;

            fLInGain        = 0.0f;
            fSInGain        = 0.0f;
            fLOutGain       = 0.0f;
            fSOutGain       = 0.0f;
            fGain           = 0.0f;
            fOldLevel       = meta::autogain::LEVEL_DFL;
            fLevel          = meta::autogain::LEVEL_DFL;

            vLBuffer        = NULL;
            vSBuffer        = NULL;
            vGainBuffer     = NULL;
            vTimePoints     = NULL;

            pBypass         = NULL;
            pLPeriod        = NULL;
            pSPeriod        = NULL;
            pWeighting      = NULL;
            pLevel          = NULL;
            pDeviation      = NULL;
            pMinGain        = NULL;
            pMaxGain        = NULL;
            pSilence        = NULL;
            pLAttack        = NULL;
            pLRelease       = NULL;
            pSAttack        = NULL;
            pSRelease       = NULL;
            pLInGain        = NULL;
            pSInGain        = NULL;
            pLOutGain       = NULL;
            pSOutGain       = NULL;
            pGain           = NULL;
            pLInGraph       = NULL;
            pSInGraph       = NULL;
            pLOutGraph      = NULL;
            pSOutGraph      = NULL;
            pGainGraph      = NULL;

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
            size_t alloc            =
                szof_channels +     // vChannels
                szof_buffer +       // vLBuffer
                szof_buffer +       // vSBuffer
                szof_buffer +       // vGainBuffer
                szof_buffer +       // vTimePoints
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
            ptr                    += szof_buffer;

            for (size_t i=0; i < nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

                c->sBypass.construct();

                c->vBuffer              = reinterpret_cast<float *>(ptr);
                ptr                    += szof_buffer;

                c->pIn                  = NULL;
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

            // Bind bypass
            pBypass             = TRACE_PORT(ports[port_id++]);
            pLPeriod            = TRACE_PORT(ports[port_id++]);
            pSPeriod            = TRACE_PORT(ports[port_id++]);
            pWeighting          = TRACE_PORT(ports[port_id++]);
            pLevel              = TRACE_PORT(ports[port_id++]);
            pDeviation          = TRACE_PORT(ports[port_id++]);
            pSilence            = TRACE_PORT(ports[port_id++]);
            pMinGain            = TRACE_PORT(ports[port_id++]);
            pMaxGain            = TRACE_PORT(ports[port_id++]);
            pLAttack            = TRACE_PORT(ports[port_id++]);
            pLRelease           = TRACE_PORT(ports[port_id++]);
            pSAttack            = TRACE_PORT(ports[port_id++]);
            pSRelease           = TRACE_PORT(ports[port_id++]);
            TRACE_PORT(ports[port_id++]); // Skip enable input gain port for long period
            TRACE_PORT(ports[port_id++]); // Skip enable input gain port for short period
            TRACE_PORT(ports[port_id++]); // Skip enable output gain port for long period
            TRACE_PORT(ports[port_id++]); // Skip enable output gain port for short period
            TRACE_PORT(ports[port_id++]); // Skip enable gain correction
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
            sGainGraph.destroy();

            sLInMeter.destroy();
            sSInMeter.destroy();
            sLOutMeter.destroy();
            sSOutMeter.destroy();

            sAutoGain.destroy();

            // Destroy channels
            if (vChannels != NULL)
            {
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c    = &vChannels[i];
                    c->sBypass.destroy();
                }
                vChannels   = NULL;
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
            sGainGraph.init(meta::autogain::MESH_POINTS, samples_per_dot);

            sLInMeter.set_sample_rate(sr);
            sSInMeter.set_sample_rate(sr);
            sLOutMeter.set_sample_rate(sr);
            sSOutMeter.set_sample_rate(sr);

            // Update sample rate for the bypass processors
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];
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

        void autogain::update_settings()
        {
            bool bypass                     = pBypass->value() >= 0.5f;
            dspu::bs::weighting_t weight    = decode_weighting(pWeighting->value());

            // Update level
            fLevel                          = dspu::lufs_to_gain(pLevel->value());

            // Configure autogain
            sAutoGain.set_deviation(
                dspu::db_to_gain(pDeviation->value()));
            sAutoGain.set_gain(
                dspu::db_to_gain(pMinGain->value()),
                dspu::db_to_gain(pMaxGain->value()));
            sAutoGain.set_short_timing(
                pSAttack->value(),
                pSRelease->value());
            sAutoGain.set_long_timing(
                pLAttack->value(),
                pLRelease->value());
            sAutoGain.set_silence_threshold(
                dspu::lufs_to_gain(pSilence->value()));

            // Set measuring period
            float l_period                  = pLPeriod->value();
            float s_period                  = pSPeriod->value();

            sLInMeter.set_period(l_period);
            sSInMeter.set_period(s_period);
            sLOutMeter.set_period(l_period);
            sSOutMeter.set_period(s_period);

            sLInMeter.set_weighting(weight);
            sSInMeter.set_weighting(weight);
            sLOutMeter.set_weighting(weight);
            sSOutMeter.set_weighting(weight);

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
            }
            else
            {
                sLInMeter.set_designation(0, dspu::bs::CHANNEL_CENTER);
                sSInMeter.set_designation(0, dspu::bs::CHANNEL_CENTER);

                sLOutMeter.set_designation(0, dspu::bs::CHANNEL_CENTER);
                sSOutMeter.set_designation(0, dspu::bs::CHANNEL_CENTER);
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
            }

            // Update bypass
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];
                c->sBypass.set_bypass(bypass);
            }
        }

        void autogain::process(size_t samples)
        {
            bind_audio_ports();
            clean_meters();

            for (size_t offset=0; offset < samples; ++offset)
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
        }

        void autogain::bind_audio_ports()
        {
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];

                c->vIn          = c->pIn->buffer<float>();
                c->vOut         = c->pOut->buffer<float>();
            }
        }

        void autogain::clean_meters()
        {
            fLInGain        = 0.0f;
            fSInGain        = 0.0f;
            fLOutGain       = 0.0f;
            fSOutGain       = 0.0f;
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
            }

            sLInMeter.process(vLBuffer, samples, dspu::bs::DBFS_TO_LUFS_SHIFT_GAIN);
            fLInGain     = lsp_max(fLInGain, dsp::max(vLBuffer, samples));
            sLInGraph.process(vLBuffer, samples);

            sSInMeter.process(vSBuffer, samples, dspu::bs::DBFS_TO_LUFS_SHIFT_GAIN);
            fSInGain     = lsp_max(fSInGain, dsp::max(vSBuffer, samples));
            sSInGraph.process(vSBuffer, samples);
        }

        void autogain::compute_gain_correction(size_t samples)
        {
            // Process autogain
            if (fOldLevel != fLevel)
            {
                dsp::lramp_set1(vGainBuffer, fOldLevel, fLevel, samples);
                sAutoGain.process(vGainBuffer, vLBuffer, vSBuffer, vGainBuffer, samples);
                fOldLevel   = fLevel;
            }
            else
                sAutoGain.process(vGainBuffer, vLBuffer, vSBuffer, fLevel, samples);

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
                dsp::mul3(c->vBuffer, c->vIn, vGainBuffer, samples);

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

                // TODO: remove this
//                dsp::copy(c->vBuffer, c->vIn, samples);

                // Apply bypass
                c->sBypass.process(c->vOut, c->vIn, c->vBuffer, samples);

                // Move pointers
                c->vIn         += samples;
                c->vOut        += samples;
            }
        }

        void autogain::output_meters()
        {
            pLInGain->set_value(fLInGain);
            pSInGain->set_value(fSInGain);
            pLOutGain->set_value(fLOutGain);
            pSOutGain->set_value(fSOutGain);
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
                dsp::copy(mesh->pvData[0], vTimePoints, meta::autogain::MESH_POINTS);
                dsp::copy(mesh->pvData[1], sSInGraph.data(), meta::autogain::MESH_POINTS);
                mesh->data(2, meta::autogain::MESH_POINTS);
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
                dsp::copy(mesh->pvData[0], vTimePoints, meta::autogain::MESH_POINTS);
                dsp::copy(mesh->pvData[1], sSOutGraph.data(), meta::autogain::MESH_POINTS);
                mesh->data(2, meta::autogain::MESH_POINTS);
            }

            mesh    = pGainGraph->buffer<plug::mesh_t>();
            if ((mesh != NULL) && (mesh->isEmpty()))
            {
                dsp::copy(mesh->pvData[0], vTimePoints, meta::autogain::MESH_POINTS);
                dsp::copy(mesh->pvData[1], sGainGraph.data(), meta::autogain::MESH_POINTS);
                mesh->data(2, meta::autogain::MESH_POINTS);
            }
        }

        void autogain::dump(dspu::IStateDumper *v) const
        {
            plug::Module::dump(v);

            // TODO
        }

    } /* namespace plugins */
} /* namespace lsp */



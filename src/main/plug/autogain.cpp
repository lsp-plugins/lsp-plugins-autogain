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

            vLBuffer        = NULL;
            vSBuffer        = NULL;
            vTimePoints     = NULL;

            pBypass         = NULL;
            pLPeriod        = NULL;
            pSPeriod        = NULL;
            pWeighting      = NULL;
            pLevel          = NULL;
            pLInGain        = NULL;
            pSInGain        = NULL;
            pLInGraph       = NULL;
            pSInGraph       = NULL;

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
                szof_buffer +       // vTimePoints
                nChannels * (
                    szof_buffer     // vBuffer
                );

            // Allocate memory-aligned data
            uint8_t *ptr            = alloc_aligned<uint8_t>(pData, alloc, OPTIMAL_ALIGN);
            if (ptr == NULL)
                return;

            if ((res = sLMeter.init(nChannels, meta::autogain::LONG_PERIOD_MAX)) != STATUS_OK)
                return;
            if ((res = sSMeter.init(nChannels, meta::autogain::SHORT_PERIOD_MAX)) != STATUS_OK)
                return;

            // Initialize pointers to channels and temporary buffer
            vChannels               = reinterpret_cast<channel_t *>(ptr);
            ptr                    += szof_channels;
            vLBuffer                = reinterpret_cast<float *>(ptr);
            ptr                    += szof_buffer;
            vSBuffer                = reinterpret_cast<float *>(ptr);
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
            TRACE_PORT(ports[port_id++]); // Skip enable input gain port for long period
            TRACE_PORT(ports[port_id++]); // Skip enable input gain port for short period
            pLInGain             = TRACE_PORT(ports[port_id++]);
            pSInGain             = TRACE_PORT(ports[port_id++]);
            pLInGraph            = TRACE_PORT(ports[port_id++]);
            pSInGraph            = TRACE_PORT(ports[port_id++]);

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
            sLMeter.destroy();
            sSMeter.destroy();

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

            sLMeter.set_sample_rate(sr);
            sSMeter.set_sample_rate(sr);
            sLInGraph.init(meta::autogain::MESH_POINTS, samples_per_dot);
            sSInGraph.init(meta::autogain::MESH_POINTS, samples_per_dot);

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

            // Set measuring period
            sLMeter.set_period(pLPeriod->value());
            sSMeter.set_period(pSPeriod->value());

            sLMeter.set_weighting(weight);
            sSMeter.set_weighting(weight);

            if (nChannels > 1)
            {
                sLMeter.set_designation(0, dspu::bs::CHANNEL_LEFT);
                sLMeter.set_designation(1, dspu::bs::CHANNEL_RIGHT);
                sSMeter.set_designation(0, dspu::bs::CHANNEL_LEFT);
                sSMeter.set_designation(1, dspu::bs::CHANNEL_RIGHT);
            }
            else
            {
                sLMeter.set_designation(0, dspu::bs::CHANNEL_CENTER);
                sSMeter.set_designation(0, dspu::bs::CHANNEL_CENTER);
            }

            for (size_t i=0; i<nChannels; ++i)
            {
                sLMeter.set_link(i, 1.0f);
                sLMeter.set_active(i, true);

                sSMeter.set_link(i, 1.0f);
                sSMeter.set_active(i, true);
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
            fLInGain         = 0.0f;
            fSInGain         = 0.0f;
        }

        void autogain::measure_input_loudness(size_t samples)
        {
            // Bind channels for analysis
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];
                sLMeter.bind(i, NULL, c->vIn, 0);
                sSMeter.bind(i, NULL, c->vIn, 0);
            }

            sLMeter.process(vLBuffer, samples, dspu::bs::DBFS_TO_LUFS_SHIFT_GAIN);
            fLInGain     = lsp_max(fLInGain, dsp::max(vLBuffer, samples));
            sLInGraph.process(vLBuffer, samples);

            sSMeter.process(vSBuffer, samples, dspu::bs::DBFS_TO_LUFS_SHIFT_GAIN);
            fSInGain     = lsp_max(fSInGain, dsp::max(vSBuffer, samples));
            sSInGraph.process(vSBuffer, samples);
        }

        void autogain::compute_gain_correction(size_t samples)
        {
            // TODO
        }

        void autogain::apply_gain_correction(size_t samples)
        {
            // TODO
        }

        void autogain::update_audio_buffers(size_t samples)
        {
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];

                // TODO: remove this after full implementation
                dsp::copy(c->vBuffer, c->vIn, samples);

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
        }

        void autogain::dump(dspu::IStateDumper *v) const
        {
            plug::Module::dump(v);

            // TODO
        }

    } /* namespace plugins */
} /* namespace lsp */



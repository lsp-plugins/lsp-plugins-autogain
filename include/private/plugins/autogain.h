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

#ifndef PRIVATE_PLUGINS_AUTOGAIN_H_
#define PRIVATE_PLUGINS_AUTOGAIN_H_

#include <lsp-plug.in/dsp-units/ctl/Bypass.h>
#include <lsp-plug.in/dsp-units/meters/LoudnessMeter.h>
#include <lsp-plug.in/dsp-units/misc/broadcast.h>
#include <lsp-plug.in/dsp-units/util/MeterGraph.h>
#include <lsp-plug.in/plug-fw/plug.h>
#include <private/meta/autogain.h>

namespace lsp
{
    namespace plugins
    {
        /**
         * Base class for the latency compensation delay
         */
        class autogain: public plug::Module
        {
            protected:
                enum mode_t
                {
                    CD_MONO,
                    CD_STEREO,
                    CD_X2_STEREO
                };

                typedef struct channel_t
                {
                    // DSP processing modules
                    dspu::Bypass            sBypass;            // Bypass

                    float                  *vIn;                // Input signal
                    float                  *vOut;               // Output signal
                    float                  *vBuffer;            // Temporary buffer for audio processing

                    plug::IPort            *pIn;                // Input port
                    plug::IPort            *pOut;               // Output port
                } channel_t;

            protected:
                dspu::MeterGraph        sInGraph;           // Loudness metering graph for input gain
                dspu::LoudnessMeter     sMeter;             // Loudness metering tool

                size_t                  nChannels;          // Number of channels
                channel_t              *vChannels;          // Delay channels

                float                   fInGain;            // Input gain meter

                float                  *vBuffer;            // Buffer for temporary data
                float                  *vTimePoints;        // Time points

                plug::IPort            *pBypass;            // Bypass
                plug::IPort            *pPeriod;            // Metering period
                plug::IPort            *pWeighting;         // Weighting function
                plug::IPort            *pLevel;             // Desired loudness level
                plug::IPort            *pInGraph;           // Input loudness graph
                plug::IPort            *pInGain;            // Input loudness meter

                uint8_t                *pData;              // Allocated data

            protected:
                static dspu::bs::weighting_t    decode_weighting(size_t weighting);

            protected:
                void                    do_destroy();
                void                    bind_audio_ports();
                void                    clean_meters();
                void                    measure_input_loudness(size_t samples);
                void                    update_audio_buffers(size_t samples);
                void                    compute_gain_correction(size_t samples);
                void                    apply_gain_correction(size_t samples);
                void                    output_mesh_data();
                void                    output_meters();

            public:
                explicit autogain(const meta::plugin_t *meta);
                autogain (const autogain &) = delete;
                autogain (autogain &&) = delete;
                virtual ~autogain() override;

                autogain & operator = (const autogain &) = delete;
                autogain & operator = (autogain &&) = delete;

                virtual void            init(plug::IWrapper *wrapper, plug::IPort **ports) override;
                virtual void            destroy() override;

            public:
                virtual void            update_sample_rate(long sr) override;
                virtual void            update_settings() override;
                virtual void            process(size_t samples) override;
                virtual void            dump(dspu::IStateDumper *v) const override;
        };

    } /* namespace plugins */
} /* namespace lsp */


#endif /* PRIVATE_PLUGINS_AUTOGAIN_H_ */


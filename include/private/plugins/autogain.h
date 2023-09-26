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
#include <lsp-plug.in/dsp-units/dynamics/AutoGain.h>
#include <lsp-plug.in/dsp-units/meters/LoudnessMeter.h>
#include <lsp-plug.in/dsp-units/misc/broadcast.h>
#include <lsp-plug.in/dsp-units/util/Delay.h>
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
                    dspu::Delay             sDelay;             // Delay

                    float                  *vIn;                // Input signal
                    float                  *vOut;               // Output signal
                    float                  *vBuffer;            // Temporary buffer for audio processing

                    plug::IPort            *pIn;                // Input port
                    plug::IPort            *pOut;               // Output port
                } channel_t;

            protected:
                dspu::MeterGraph        sLInGraph;          // Loudness metering graph for long input gain
                dspu::MeterGraph        sSInGraph;          // Loudness metering graph for short input gain
                dspu::MeterGraph        sLOutGraph;         // Loudness metering graph for long output gain
                dspu::MeterGraph        sSOutGraph;         // Loudness metering graph for short output gain
                dspu::MeterGraph        sGainGraph;         // Gain correction graph
                dspu::LoudnessMeter     sLInMeter;          // Input loudness metering tool for long period
                dspu::LoudnessMeter     sSInMeter;          // Input loudness metering tool for short period
                dspu::LoudnessMeter     sLOutMeter;         // Output loudness metering tool for long period
                dspu::LoudnessMeter     sSOutMeter;         // Output loudness metering tool for short period
                dspu::AutoGain          sAutoGain;          // Auto-gain

                size_t                  nChannels;          // Number of channels
                channel_t              *vChannels;          // Delay channels

                float                   fLInGain;           // Input gain meter for long period
                float                   fSInGain;           // Input gain meter for short period
                float                   fLOutGain;          // Output gain meter for long period
                float                   fSOutGain;          // Output gain meter for short period
                float                   fGain;              // Gain correction meter
                float                   fOldLevel;          // Old level value
                float                   fLevel;             // Current level value

                float                  *vLBuffer;           // Buffer for long input gain
                float                  *vSBuffer;           // Buffer for short input gain
                float                  *vGainBuffer;        // Buffer for gain correction
                float                  *vTimePoints;        // Time points

                plug::IPort            *pBypass;            // Bypass
                plug::IPort            *pLPeriod;           // Metering long period
                plug::IPort            *pSPeriod;           // Metering short period
                plug::IPort            *pWeighting;         // Weighting function
                plug::IPort            *pLevel;             // Desired loudness level
                plug::IPort            *pDeviation;         // Deviation
                plug::IPort            *pMinGain;           // Minimum control gain
                plug::IPort            *pMaxGain;           // Maximum control gain
                plug::IPort            *pSilence;           // Silence threshold
                plug::IPort            *pLAttack;           // Long attack time
                plug::IPort            *pLRelease;          // Long release time
                plug::IPort            *pSAttack;           // Short attack time
                plug::IPort            *pSRelease;          // Short release time
                plug::IPort            *pLInGain;           // Input loudness meter for long period
                plug::IPort            *pSInGain;           // Input loudness meter for long period
                plug::IPort            *pLOutGain;          // Output loudness meter for long period
                plug::IPort            *pSOutGain;          // Output loudness meter for long period
                plug::IPort            *pGain;              // Gain correction level
                plug::IPort            *pLInGraph;          // Input loudness graph for long period
                plug::IPort            *pSInGraph;          // Input loudness graph for short period
                plug::IPort            *pLOutGraph;         // Output loudness graph for long period
                plug::IPort            *pSOutGraph;         // Output loudness graph for short period
                plug::IPort            *pGainGraph;         // Gain correction graph

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


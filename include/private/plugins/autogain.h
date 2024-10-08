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
#include <lsp-plug.in/plug-fw/core/IDBuffer.h>
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

                enum gcontrol_type_t
                {
                    GCT_LONG_GROW,
                    GCT_LONG_FALL,
                    GCT_SHORT_GROW,
                    GCT_SHORT_FALL,

                    GCT_TOTAL
                };

                typedef struct gcontrol_t
                {
                    plug::IPort            *pValue;             // Numerator of the gain speed
                    plug::IPort            *pPeroid;            // Denominator of the gain speed
                } gcontrol_t;

                typedef struct channel_t
                {
                    // DSP processing modules
                    dspu::Bypass            sBypass;            // Bypass
                    dspu::Delay             sDelay;             // Delay

                    float                  *vIn;                // Input signal
                    float                  *vScIn;              // Sidechain input
                    float                  *vShmIn;             // Shared memory input
                    float                  *vOut;               // Output signal
                    float                  *vBuffer;            // Temporary buffer for audio processing

                    plug::IPort            *pIn;                // Input port
                    plug::IPort            *pScIn;              // Sidechain input port
                    plug::IPort            *pShmIn;             // Shared memory input port
                    plug::IPort            *pOut;               // Output port
                } channel_t;

            protected:
                dspu::MeterGraph        sLInGraph;          // Loudness metering graph for long input gain
                dspu::MeterGraph        sSInGraph;          // Loudness metering graph for short input gain
                dspu::MeterGraph        sLOutGraph;         // Loudness metering graph for long output gain
                dspu::MeterGraph        sSOutGraph;         // Loudness metering graph for short output gain
                dspu::MeterGraph        sLScGraph;          // Sidechain metering graph for long output gain
                dspu::MeterGraph        sSScGraph;          // Sidechain metering graph for short output gain
                dspu::MeterGraph        sGainGraph;         // Gain correction graph
                dspu::LoudnessMeter     sLInMeter;          // Input loudness metering tool for long period
                dspu::LoudnessMeter     sSInMeter;          // Input loudness metering tool for short period
                dspu::LoudnessMeter     sLOutMeter;         // Output loudness metering tool for long period
                dspu::LoudnessMeter     sSOutMeter;         // Output loudness metering tool for short period
                dspu::LoudnessMeter     sLScMeter;          // Sidechain loudness metering for the long period
                dspu::LoudnessMeter     sSScMeter;          // Sidechain loudness metering for the short period
                dspu::AutoGain          sAutoGain;          // Auto-gain

                size_t                  nChannels;          // Number of channels
                size_t                  enScMode;           // Sidechain mode
                bool                    bSidechain;         // Sidechain is available
                channel_t              *vChannels;          // Delay channels

                float                   fLInGain;           // Input gain meter for long period
                float                   fSInGain;           // Input gain meter for short period
                float                   fLOutGain;          // Output gain meter for long period
                float                   fSOutGain;          // Output gain meter for short period
                float                   fLScGain;           // Output gain meter for long sidechain
                float                   fSScGain;           // Output gain meter for short sidechain
                float                   fGain;              // Gain correction meter
                float                   fOldLevel;          // Old level value
                float                   fLevel;             // Current level value
                float                   fOldPreamp;         // Old sidechain preamp
                float                   fPreamp;            // Actual sidechain preamp

                float                  *vLBuffer;           // Buffer for long input gain
                float                  *vSBuffer;           // Buffer for short input gain
                float                  *vGainBuffer;        // Buffer for gain correction
                float                  *vEmptyBuffer;       // Empty buffer for audio fallback
                float                  *vTimePoints;        // Time points

                plug::IPort            *pBypass;            // Bypass
                plug::IPort            *pScMode;            // Sidechain mode
                plug::IPort            *pScPreamp;          // Sidechain preamp
                plug::IPort            *pLookahead;         // Lookahead
                plug::IPort            *pLPeriod;           // Metering long period
                plug::IPort            *pSPeriod;           // Metering short period
                plug::IPort            *pWeighting;         // Weighting function
                plug::IPort            *pLevel;             // Desired loudness level
                plug::IPort            *pDeviation;         // Deviation
                plug::IPort            *pSilence;           // Silence threshold
                plug::IPort            *pAmpOn;             // Maximum amplification gain limitation switch
                plug::IPort            *pAmpGain;           // Maximum amplification gain
                plug::IPort            *pQAmp;              // Quick amplifier option
                gcontrol_t              vGainCtl[GCT_TOTAL];// Gain controls
                plug::IPort            *pLInGain;           // Input loudness meter for long period
                plug::IPort            *pSInGain;           // Input loudness meter for long period
                plug::IPort            *pLOutGain;          // Output loudness meter for long period
                plug::IPort            *pSOutGain;          // Output loudness meter for long period
                plug::IPort            *pLScGain;           // Sidechain loudness meter for long period
                plug::IPort            *pSScGain;           // Sidechain loudness meter for long period
                plug::IPort            *pGain;              // Gain correction level
                plug::IPort            *pLInGraph;          // Input loudness graph for long period
                plug::IPort            *pSInGraph;          // Input loudness graph for short period
                plug::IPort            *pLOutGraph;         // Output loudness graph for long period
                plug::IPort            *pSOutGraph;         // Output loudness graph for short period
                plug::IPort            *pLScGraph;          // Sidechain loudness graph for long period
                plug::IPort            *pSScGraph;          // Sidechain loudness graph for short period
                plug::IPort            *pGainGraph;         // Gain correction graph

                core::IDBuffer         *pIDisplay;          // Inline display buffer

                uint8_t                *pData;              // Allocated data

            protected:
                static dspu::bs::weighting_t    decode_weighting(size_t weighting);
                meta::autogain::scmode_t        decode_sidechain_mode(size_t mode);

            protected:
                const float            *select_buffer(const channel_t *c) const;
                void                    do_destroy();
                void                    bind_audio_ports();
                void                    clean_meters();
                void                    measure_input_loudness(size_t samples);
                void                    update_audio_buffers(size_t samples);
                void                    compute_gain_correction(size_t samples);
                void                    apply_gain_correction(size_t samples);
                void                    output_mesh_data();
                void                    output_meters();
                inline float            calc_gain_speed(gcontrol_type_t type);

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
                virtual bool            inline_display(plug::ICanvas *cv, size_t width, size_t height) override;
                virtual void            dump(dspu::IStateDumper *v) const override;
        };

    } /* namespace plugins */
} /* namespace lsp */


#endif /* PRIVATE_PLUGINS_AUTOGAIN_H_ */


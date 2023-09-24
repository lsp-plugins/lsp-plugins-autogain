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

#ifndef PRIVATE_META_AUTOGAIN_H_
#define PRIVATE_META_AUTOGAIN_H_

#include <lsp-plug.in/plug-fw/meta/types.h>
#include <lsp-plug.in/plug-fw/const.h>

namespace lsp
{
    //-------------------------------------------------------------------------
    // Plugin metadata
    namespace meta
    {
        typedef struct autogain
        {
            static constexpr float  LONG_PERIOD_MIN     = 100.0f;
            static constexpr float  LONG_PERIOD_MAX     = 2000.0f;
            static constexpr float  LONG_PERIOD_STEP    = 0.005f;
            static constexpr float  LONG_PERIOD_DFL     = 400.0f;

            static constexpr float  SHORT_PERIOD_MIN    = 5.0f;
            static constexpr float  SHORT_PERIOD_MAX    = 100.0f;
            static constexpr float  SHORT_PERIOD_STEP   = 0.0025f;
            static constexpr float  SHORT_PERIOD_DFL    = 20.0f;

            static constexpr float  LONG_ATTACK_MIN     = 0.0f;
            static constexpr float  LONG_ATTACK_MAX     = 1000.0f;
            static constexpr float  LONG_ATTACK_STEP    = 0.0001f;
            static constexpr float  LONG_ATTACK_DFL     = 50.0f;

            static constexpr float  SHORT_ATTACK_MIN    = 0.0f;
            static constexpr float  SHORT_ATTACK_MAX    = 50.0f;
            static constexpr float  SHORT_ATTACK_STEP   = 0.0001f;
            static constexpr float  SHORT_ATTACK_DFL    = 10.0f;

            static constexpr float  LONG_RELEASE_MIN   = 0.0f;
            static constexpr float  LONG_RELEASE_MAX   = 5000.0f;
            static constexpr float  LONG_RELEASE_STEP  = 0.0001f;
            static constexpr float  LONG_RELEASE_DFL   = 500.0f;

            static constexpr float  SHORT_RELEASE_MIN   = 0.0f;
            static constexpr float  SHORT_RELEASE_MAX   = 200.0f;
            static constexpr float  SHORT_RELEASE_STEP  = 0.0001f;
            static constexpr float  SHORT_RELEASE_DFL   = 50.0f;

            static constexpr float  DEVIATION_MIN       = 0.0f;
            static constexpr float  DEVIATION_MAX       = 24.0f;
            static constexpr float  DEVIATION_STEP      = 0.01f;
            static constexpr float  DEVIATION_DFL       = 6.0f;

            static constexpr float  SILENCE_MIN         = -120.0f;
            static constexpr float  SILENCE_MAX         = -24.0f;
            static constexpr float  SILENCE_STEP        = 0.01f;
            static constexpr float  SILENCE_DFL         = -76.0f;

            static constexpr float  MIN_GAIN_MIN        = -84.0f;
            static constexpr float  MIN_GAIN_MAX        = 0.0f;
            static constexpr float  MIN_GAIN_STEP       = 0.01f;
            static constexpr float  MIN_GAIN_DFL        = -48.0f;

            static constexpr float  MAX_GAIN_MIN        = 0.0f;
            static constexpr float  MAX_GAIN_MAX        = 84.0f;
            static constexpr float  MAX_GAIN_STEP       = 0.01f;
            static constexpr float  MAX_GAIN_DFL        = 48.0f;

            static constexpr float  LEVEL_MIN           = -60.0f;
            static constexpr float  LEVEL_MAX           = 0.0f;
            static constexpr float  LEVEL_STEP          = 0.05f;
            static constexpr float  LEVEL_DFL           = -23.0f;

            static constexpr float  MESH_TIME           = 2.0f;
            static constexpr size_t MESH_POINTS         = 640.0f;

            static constexpr float  DELAY_OUT_MAX_TIME  = 10000.0f;

            enum weighting_t
            {
                WEIGHT_NONE,
                WEIGHT_A,
                WEIGHT_B,
                WEIGHT_C,
                WEIGHT_D,
                WEIGHT_K,

                WEIGHT_DFL = WEIGHT_K
            };
        } autogain;

        // Plugin type metadata
        extern const plugin_t autogain_mono;
        extern const plugin_t autogain_stereo;

    } /* namespace meta */
} /* namespace lsp */

#endif /* PRIVATE_META_AUTOGAIN_H_ */

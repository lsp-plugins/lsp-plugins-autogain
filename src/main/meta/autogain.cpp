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

#include <lsp-plug.in/plug-fw/meta/ports.h>
#include <lsp-plug.in/shared/meta/developers.h>
#include <private/meta/autogain.h>

#define LSP_PLUGINS_AUTOGAIN_VERSION_MAJOR       1
#define LSP_PLUGINS_AUTOGAIN_VERSION_MINOR       0
#define LSP_PLUGINS_AUTOGAIN_VERSION_MICRO       0

#define LSP_PLUGINS_AUTOGAIN_VERSION  \
    LSP_MODULE_VERSION( \
        LSP_PLUGINS_AUTOGAIN_VERSION_MAJOR, \
        LSP_PLUGINS_AUTOGAIN_VERSION_MINOR, \
        LSP_PLUGINS_AUTOGAIN_VERSION_MICRO  \
    )

namespace lsp
{
    namespace meta
    {
        static const port_item_t weighting_modes[] =
        {
            { "None",           "autogain.weighting.none"   },
            { "A-Weighted",     "autogain.weighting.a"      },
            { "B-Weighted",     "autogain.weighting.b"      },
            { "C-Weighted",     "autogain.weighting.c"      },
            { "D-Weighted",     "autogain.weighting.d"      },
            { "K-Weighted",     "autogain.weighting.k"      },
            { NULL, NULL }
        };

        #define AUTOGAIN_COMMON \
            BYPASS, \
            LOG_CONTROL("lperiod", "Loudness measuring long period", U_MSEC, meta::autogain::LONG_PERIOD), \
            LOG_CONTROL("speriod", "Loudness measuring short period", U_MSEC, meta::autogain::SHORT_PERIOD), \
            COMBO("weight", "Weighting function", meta::autogain::WEIGHT_DFL, weighting_modes), \
            CONTROL("level", "Desired loudness level", U_LUFS, meta::autogain::LEVEL), \
            CONTROL("dev", "Level deviation", U_DB, meta::autogain::DEVIATION), \
            CONTROL("silence", "The level of silence", U_LUFS, meta::autogain::SILENCE), \
            CONTROL("g_min", "Minimum control gain", U_DB, meta::autogain::MIN_GAIN), \
            CONTROL("g_max", "Maximum control gain", U_DB, meta::autogain::MAX_GAIN), \
            LOG_CONTROL("att_l", "Long attack time", U_MSEC, meta::autogain::LONG_ATTACK), \
            LOG_CONTROL("rel_l", "Long release time", U_MSEC, meta::autogain::LONG_RELEASE), \
            LOG_CONTROL("att_s", "Short attack time", U_MSEC, meta::autogain::SHORT_ATTACK), \
            LOG_CONTROL("rel_s", "Short release time", U_MSEC, meta::autogain::SHORT_RELEASE), \
            \
            SWITCH("e_in_s", "Input metering enable for short period", 1.0f), \
            SWITCH("e_in_l", "Input metering enable for long period", 1.0f), \
            SWITCH("e_out_s", "Output metering enable for short period", 1.0f), \
            SWITCH("e_out_l", "Output metering enable for long period", 1.0f), \
            SWITCH("e_g", "Gain correction metering", 1.0f), \
            \
            METER_GAIN("g_in_s", "Input loudness meter for short period", GAIN_AMP_P_48_DB), \
            METER_GAIN("g_in_l", "Input loudness meter for long period", GAIN_AMP_P_48_DB), \
            METER_GAIN("g_out_s", "Output loudness meter for short period", GAIN_AMP_P_48_DB), \
            METER_GAIN("g_out_l", "Output loudness meter for long period", GAIN_AMP_P_48_DB), \
            METER_GAIN("g_g", "Gain correction meter", GAIN_AMP_P_120_DB), \
            \
            MESH("gr_in_s", "Input loudness graph for short period", 2, meta::autogain::MESH_POINTS), \
            MESH("gr_in_l", "Input loudness graph for long period", 2, meta::autogain::MESH_POINTS), \
            MESH("gr_out_s", "Output loudness graph for short period", 2, meta::autogain::MESH_POINTS), \
            MESH("gr_out_l", "Output loudness graph for long period", 2, meta::autogain::MESH_POINTS), \
            MESH("gr_g", "Gain correction graph", 2, meta::autogain::MESH_POINTS)


        static const port_t autogain_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,
            AUTOGAIN_COMMON,

            PORTS_END
        };

        static const port_t autogain_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            AUTOGAIN_COMMON,

            PORTS_END
        };

        static const int plugin_classes[]       = { C_ENVELOPE, -1 };
        static const int clap_features_mono[]   = { CF_AUDIO_EFFECT, CF_UTILITY, CF_MONO, -1 };
        static const int clap_features_stereo[] = { CF_AUDIO_EFFECT, CF_UTILITY, CF_STEREO, -1 };

        const meta::bundle_t autogain_bundle =
        {
            "autogain",
            "Automatic Gain Control",
            B_UTILITIES,
            "", // TODO: provide ID of the video on YouTube
            "This plugin allows to stick the loudness of the audio to the desired level"
        };

        const plugin_t autogain_mono =
        {
            "Autogain Mono",
            "Autogain Mono",
            "AG1M",
            &developers::v_sadovnikov,
            "autogain_mono",
            LSP_LV2_URI("autogain_mono"),
            LSP_LV2UI_URI("autogain_mono"),
            "ag1m",
            LSP_LADSPA_AUTOGAIN_BASE + 0,
            LSP_LADSPA_URI("autogain_mono"),
            LSP_CLAP_URI("autogain_mono"),
            LSP_PLUGINS_AUTOGAIN_VERSION,
            plugin_classes,
            clap_features_mono,
            E_DUMP_STATE,
            autogain_mono_ports,
            "util/autogain/mono.xml",
            NULL,
            mono_plugin_port_groups,
            &autogain_bundle
        };

        const plugin_t autogain_stereo =
        {
            "Autogain Stereo",
            "Autogain Stereo",
            "AG1S",
            &developers::v_sadovnikov,
            "autogain_stereo",
            LSP_LV2_URI("autogain_stereo"),
            LSP_LV2UI_URI("autogain_stereo"),
            "ag1s",
            LSP_LADSPA_AUTOGAIN_BASE + 1,
            LSP_LADSPA_URI("autogain_stereo"),
            LSP_CLAP_URI("autogain_stereo"),
            LSP_PLUGINS_AUTOGAIN_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_DUMP_STATE,
            autogain_stereo_ports,
            "util/autogain/stereo.xml",
            NULL,
            stereo_plugin_port_groups,
            &autogain_bundle
        };
    } /* namespace meta */
} /* namespace lsp */




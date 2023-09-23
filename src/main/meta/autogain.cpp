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
            LOG_CONTROL("period", "Loudness measuring period", U_MSEC, meta::autogain::PERIOD),     \
            COMBO("weight", "Weighting function", meta::autogain::WEIGHT_DFL, weighting_modes),     \
            CONTROL("level", "Desired loudness level", U_LUFS, meta::autogain::LEVEL),              \
            SWITCH("e_in", "Input metering enable", 1.0f), \
            METER_GAIN("g_in", "Input loudness meter", GAIN_AMP_P_48_DB), \
            MESH("gr_in", "Input loudness graph", 2, meta::autogain::MESH_POINTS)


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




/*
 * Copyright (C) 2024 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2024 Vladimir Sadovnikov <sadko4u@gmail.com>
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
#define LSP_PLUGINS_AUTOGAIN_VERSION_MICRO       10

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
        static const port_item_t sc_modes[] =
        {
            { "Internal",       "autogain.sc.internal"      },
            { "Control Link",   "autogain.sc.control_link"  },
            { "Match Link",     "autogain.sc.match_link"    },
            { NULL, NULL }
        };

        static const port_item_t sc_modes_sc[] =
        {
            { "Internal",       "autogain.sc.internal"      },
            { "Control",        "autogain.sc.control"       },
            { "Match",          "autogain.sc.match"         },
            { "Control Link",   "autogain.sc.control_link"  },
            { "Match Link",     "autogain.sc.match_link"    },
            { NULL, NULL }
        };

        static const port_item_t speed_numerators[] =
        {
            { "0.1 dB",         "autogain.numerator.0_1db"  },
            { "0.5 dB",         "autogain.numerator.0_5db"  },
            { "1 dB",           "autogain.numerator.1db"    },
            { "3 dB",           "autogain.numerator.3db"    },
            { "6 dB",           "autogain.numerator.6db"    },
            { "9 dB",           "autogain.numerator.9db"    },
            { "10 dB",          "autogain.numerator.10db"   },
            { "12 dB",          "autogain.numerator.12db"   },
            { "15 dB",          "autogain.numerator.15db"   },
            { "18 dB",          "autogain.numerator.18db"   },
            { "20 dB",          "autogain.numerator.20db"   },
            { "21 dB",          "autogain.numerator.21db"   },
            { "24 dB",          "autogain.numerator.24db"   },
            { NULL, NULL }
        };

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

        #define AUTOGAIN_LINK_MONO \
            OPT_RETURN_NAME("link", "Side-chain shared memory link name"), \
            OPT_AUDIO_RETURN("scl", "Side-chain shared memory link input", 0, "link")

        #define AUTOGAIN_LINK_STEREO \
            OPT_RETURN_NAME("link", "Side-chain shared memory link name"), \
            OPT_AUDIO_RETURN("scl_l", "Side-chain shared memory link input Left", 0, "link"), \
            OPT_AUDIO_RETURN("scl_r", "Side-chain shared memory link input Right", 1, "link")

        #define AUTOGAIN_COMMON_SC(combo, combo_dfl) \
            CONTROL("preamp", "Sidechain preamp", U_DB, meta::autogain::SC_PREAMP), \
            CONTROL("lkahead", "Sidechain lookahead", U_MSEC, meta::autogain::SC_LOOKAHEAD), \
            COMBO("scmode", "Sidechain mode", combo_dfl, combo), \
            SWITCH("e_sc_l", "Sidechain metering enable for long period", 1.0f), \
            SWITCH("e_sc_s", "Sidechain metering enable for short period", 1.0f), \
            METER_GAIN("g_sc_l", "Sidechain loudness meter for long period", GAIN_AMP_P_48_DB), \
            METER_GAIN("g_sc_s", "Sidechain loudness meter for short period", GAIN_AMP_P_48_DB), \
            MESH("gr_sc_l", "Sidechain loudness graph for long period", 2, meta::autogain::MESH_POINTS), \
            MESH("gr_sc_s", "Sidechain loudness graph for short period", 2, meta::autogain::MESH_POINTS + 2)

        #define AUTOGAIN_INT_SC \
            AUTOGAIN_COMMON_SC(sc_modes, meta::autogain::SCMODE_DFL)

        #define AUTOGAIN_EXT_SC \
            AUTOGAIN_COMMON_SC(sc_modes_sc, meta::autogain::SCMODE_DFL_SC)

        #define AUTOGAIN_COMMON \
            LOG_CONTROL("lperiod", "Loudness measuring long period", U_MSEC, meta::autogain::LONG_PERIOD), \
            LOG_CONTROL("speriod", "Loudness measuring short period", U_MSEC, meta::autogain::SHORT_PERIOD), \
            COMBO("weight", "Weighting function", meta::autogain::WEIGHT_DFL, weighting_modes), \
            CONTROL("level", "Desired loudness level", U_LUFS, meta::autogain::LEVEL), \
            CONTROL("drift", "Level drift", U_DB, meta::autogain::DEVIATION), \
            CONTROL("silence", "The level of silence", U_LUFS, meta::autogain::SILENCE), \
            SWITCH("max_on", "Enable maximum amplification gain limitation", 0.0f), \
            CONTROL("max_amp", "The maximum amplification gain", U_DB, meta::autogain::MAX_GAIN), \
            \
            SWITCH("qamp", "Enable quick amplifier", 0.0f), \
            COMBO("vgrow_l", "Long gain grow amount", meta::autogain::NUM_DFL, speed_numerators), \
            LOG_CONTROL("tgrow_l", "Long gain grow time", U_MSEC, meta::autogain::LONG_GROW), \
            COMBO("vfall_l", "Long gain fall amount", meta::autogain::NUM_DFL, speed_numerators), \
            LOG_CONTROL("tfall_l", "Long gain fall time", U_MSEC, meta::autogain::LONG_FALL), \
            COMBO("vgrow_s", "Short gain grow amount", meta::autogain::NUM_DFL, speed_numerators), \
            LOG_CONTROL("tgrow_s", "Short gain grow time", U_MSEC, meta::autogain::SHORT_GROW), \
            COMBO("vfall_s", "Short gain fall amount", meta::autogain::NUM_DFL, speed_numerators), \
            LOG_CONTROL("tfall_s", "Short gain fall time", U_MSEC, meta::autogain::SHORT_FALL), \
            \
            SWITCH("e_in_l", "Input metering enable for long period", 1.0f), \
            SWITCH("e_in_s", "Input metering enable for short period", 1.0f), \
            SWITCH("e_out_l", "Output metering enable for long period", 1.0f), \
            SWITCH("e_out_s", "Output metering enable for short period", 1.0f), \
            SWITCH("e_g", "Gain correction metering", 1.0f), \
            \
            METER_GAIN("g_in_l", "Input loudness meter for long period", GAIN_AMP_P_48_DB), \
            METER_GAIN("g_in_s", "Input loudness meter for short period", GAIN_AMP_P_48_DB), \
            METER_GAIN("g_out_l", "Output loudness meter for long period", GAIN_AMP_P_48_DB), \
            METER_GAIN("g_out_s", "Output loudness meter for short period", GAIN_AMP_P_48_DB), \
            METER_GAIN("g_g", "Gain correction meter", GAIN_AMP_P_120_DB), \
            \
            MESH("gr_in_l", "Input loudness graph for long period", 2, meta::autogain::MESH_POINTS), \
            MESH("gr_in_s", "Input loudness graph for short period", 2, meta::autogain::MESH_POINTS + 2), \
            MESH("gr_out_l", "Output loudness graph for long period", 2, meta::autogain::MESH_POINTS), \
            MESH("gr_out_s", "Output loudness graph for short period", 2, meta::autogain::MESH_POINTS + 2), \
            MESH("gr_g", "Gain correction graph", 2, meta::autogain::MESH_POINTS + 4)


        static const port_t autogain_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,
            BYPASS,
            AUTOGAIN_LINK_MONO,
            AUTOGAIN_INT_SC,
            AUTOGAIN_COMMON,

            PORTS_END
        };

        static const port_t autogain_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            BYPASS,
            AUTOGAIN_LINK_STEREO,
            AUTOGAIN_INT_SC,
            AUTOGAIN_COMMON,

            PORTS_END
        };

        static const port_t sc_autogain_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,
            PORTS_MONO_SIDECHAIN,
            BYPASS,
            AUTOGAIN_LINK_MONO,
            AUTOGAIN_EXT_SC,
            AUTOGAIN_COMMON,

            PORTS_END
        };

        static const port_t sc_autogain_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            PORTS_STEREO_SIDECHAIN,
            BYPASS,
            AUTOGAIN_LINK_STEREO,
            AUTOGAIN_EXT_SC,
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
            "i_10kAtZmJU",
            "This plugin allows to stick the loudness of the audio to the desired level"
        };

        const plugin_t autogain_mono =
        {
            "Autogain Mono",
            "Autogain Mono",
            "Autogain Mono",
            "AG1M",
            &developers::v_sadovnikov,
            "autogain_mono",
            {
                LSP_LV2_URI("autogain_mono"),
                LSP_LV2UI_URI("autogain_mono"),
                "ag1m",
                LSP_VST3_UID("ag1m    ag1m"),
                LSP_VST3UI_UID("ag1m    ag1m"),
                LSP_LADSPA_AUTOGAIN_BASE + 0,
                LSP_LADSPA_URI("autogain_mono"),
                LSP_CLAP_URI("autogain_mono"),
                LSP_GST_UID("autogain_mono"),
            },
            LSP_PLUGINS_AUTOGAIN_VERSION,
            plugin_classes,
            clap_features_mono,
            E_DUMP_STATE | E_INLINE_DISPLAY,
            autogain_mono_ports,
            "util/autogain.xml",
            NULL,
            mono_plugin_port_groups,
            &autogain_bundle
        };

        const plugin_t autogain_stereo =
        {
            "Autogain Stereo",
            "Autogain Stereo",
            "Autogain Stereo",
            "AG1S",
            &developers::v_sadovnikov,
            "autogain_stereo",
            {
                LSP_LV2_URI("autogain_stereo"),
                LSP_LV2UI_URI("autogain_stereo"),
                "ag1s",
                LSP_VST3_UID("ag1s    ag1s"),
                LSP_VST3UI_UID("ag1s    ag1s"),
                LSP_LADSPA_AUTOGAIN_BASE + 1,
                LSP_LADSPA_URI("autogain_stereo"),
                LSP_CLAP_URI("autogain_stereo"),
                LSP_GST_UID("autogain_stereo"),
            },
            LSP_PLUGINS_AUTOGAIN_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_DUMP_STATE | E_INLINE_DISPLAY,
            autogain_stereo_ports,
            "util/autogain.xml",
            NULL,
            stereo_plugin_port_groups,
            &autogain_bundle
        };

        const plugin_t sc_autogain_mono =
        {
            "Sidechain Autogain Mono",
            "Sidechain Autogain Mono",
            "Sidechain Autogain Mono",
            "SCAG1M",
            &developers::v_sadovnikov,
            "sc_autogain_mono",
            {
                LSP_LV2_URI("sc_autogain_mono"),
                LSP_LV2UI_URI("sc_autogain_mono"),
                "ag1M",
                LSP_VST3_UID("scag1m  ag1M"),
                LSP_VST3UI_UID("scag1m  ag1M"),
                LSP_LADSPA_AUTOGAIN_BASE + 2,
                LSP_LADSPA_URI("sc_autogain_mono"),
                LSP_CLAP_URI("sc_autogain_mono"),
                LSP_GST_UID("sc_autogain_mono"),
            },
            LSP_PLUGINS_AUTOGAIN_VERSION,
            plugin_classes,
            clap_features_mono,
            E_DUMP_STATE | E_INLINE_DISPLAY,
            sc_autogain_mono_ports,
            "util/autogain.xml",
            NULL,
            mono_plugin_sidechain_port_groups,
            &autogain_bundle
        };

        const plugin_t sc_autogain_stereo =
        {
            "Sidechain Autogain Stereo",
            "Sidechain Autogain Stereo",
            "Sidechain Autogain Stereo",
            "SCAG1S",
            &developers::v_sadovnikov,
            "sc_autogain_stereo",
            {
                LSP_LV2_URI("sc_autogain_stereo"),
                LSP_LV2UI_URI("sc_autogain_stereo"),
                "ag1S",
                LSP_VST3_UID("scag1s  ag1S"),
                LSP_VST3UI_UID("scag1s  ag1S"),
                LSP_LADSPA_AUTOGAIN_BASE + 3,
                LSP_LADSPA_URI("sc_autogain_stereo"),
                LSP_CLAP_URI("sc_autogain_stereo"),
                LSP_GST_UID("sc_autogain_stereo"),
            },
            LSP_PLUGINS_AUTOGAIN_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_DUMP_STATE | E_INLINE_DISPLAY,
            sc_autogain_stereo_ports,
            "util/autogain.xml",
            NULL,
            stereo_plugin_sidechain_port_groups,
            &autogain_bundle
        };
    } /* namespace meta */
} /* namespace lsp */




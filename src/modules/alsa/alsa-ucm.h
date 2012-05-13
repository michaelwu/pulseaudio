#ifndef foopulseucmhfoo
#define foopulseucmhfoo

/***
  This file is part of PulseAudio.

  Copyright 2011 Wolfson Microelectronics PLC
  Author Margarita Olaya <magi@slimlogic.co.uk>
  Copyright 2012 Feng Wei <feng.wei@linaro.org>, Linaro

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2.1 of the License,
  or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#include <asoundlib.h>
#include <use-case.h>

typedef struct pa_core pa_core;
typedef struct pa_device_port pa_device_port;
typedef struct pa_alsa_mapping pa_alsa_mapping;

typedef struct pa_alsa_ucm_verb pa_alsa_ucm_verb;
typedef struct pa_alsa_ucm_modifier pa_alsa_ucm_modifier;
typedef struct pa_alsa_ucm_device pa_alsa_ucm_device;
typedef struct pa_alsa_ucm_config pa_alsa_ucm_config;
typedef struct pa_alsa_ucm_mapping_context pa_alsa_ucm_mapping_context;
typedef struct pa_alsa_port_data_ucm pa_alsa_port_data_ucm;
typedef struct pa_alsa_jack pa_alsa_jack;

int  ucm_set_profile(struct pa_alsa_ucm_config *ucm, const char *new_profile, const char *old_profile);
void ucm_free(struct pa_alsa_ucm_config *ucm);
void ucm_mapping_context_free(pa_alsa_ucm_mapping_context *context);
pa_alsa_profile_set* ucm_add_profile_set(struct pa_alsa_ucm_config *ucm, pa_channel_map *default_channel_map);
int  ucm_get_verb(snd_use_case_mgr_t *uc_mgr, const char *verb_name, const char *verb_desc, struct pa_alsa_ucm_verb ** p_verb);
void ucm_add_ports(pa_hashmap **p, pa_proplist *proplist, pa_alsa_ucm_mapping_context *context, int is_sink, pa_card *card);
void ucm_add_ports_combination(pa_hashmap *hash, pa_alsa_ucm_mapping_context *context, int is_sink, int *dev_indices,
        int dev_num, int map_index, pa_hashmap *ports, pa_card_profile *cp, pa_core *core);
int  ucm_set_port(pa_alsa_ucm_mapping_context *context, pa_device_port *port, int is_sink);

void ucm_roled_stream_begin(pa_alsa_ucm_config *ucm, const char *role, const char *mapping_name, int is_sink);
void ucm_roled_stream_end(pa_alsa_ucm_config *ucm, const char *role, const char *mapping_name, int is_sink);

/* UCM modifier action direction */
enum {
    PA_ALSA_UCM_DIRECT_NONE = 0,
    PA_ALSA_UCM_DIRECT_SINK,
    PA_ALSA_UCM_DIRECT_SOURCE
};

/* UCM - Use Case Manager is available on some audio cards */

struct pa_alsa_ucm_device {
    PA_LLIST_FIELDS(pa_alsa_ucm_device);
    pa_proplist *proplist;
    unsigned playback_priority;
    unsigned capture_priority;
    unsigned playback_channels;
    unsigned capture_channels;
    pa_alsa_mapping *playback_mapping;
    pa_alsa_mapping *capture_mapping;
    int n_confdev;
    int n_suppdev;
    char **conflicting_devices;
    char **supported_devices;
    pa_alsa_jack *jack;
};

struct pa_alsa_ucm_modifier {
    PA_LLIST_FIELDS(pa_alsa_ucm_modifier);
    pa_proplist *proplist;
    int n_confdev;
    int n_suppdev;
    const char **conflicting_devices;
    const char **supported_devices;
    int action_direct;
    char *media_role;

    /* runtime variable */
    int enabled_counter;
};

struct pa_alsa_ucm_verb {
    PA_LLIST_FIELDS(pa_alsa_ucm_verb);
    pa_proplist *proplist;
    PA_LLIST_HEAD(pa_alsa_ucm_device, devices);
    PA_LLIST_HEAD(pa_alsa_ucm_modifier, modifiers);
};

struct pa_alsa_ucm_config {
    pa_core *core;
    snd_use_case_mgr_t *ucm_mgr;
    pa_alsa_ucm_verb *active_verb;

    PA_LLIST_HEAD(pa_alsa_ucm_verb, verbs);
    PA_LLIST_HEAD(pa_alsa_jack, jacks);
};

struct pa_alsa_ucm_mapping_context {
    pa_alsa_ucm_config *ucm;
    int direction;
    int ucm_devices_num;
    pa_alsa_ucm_device **ucm_devices;
};

struct pa_alsa_port_data_ucm {
    int dummy;
};

#endif

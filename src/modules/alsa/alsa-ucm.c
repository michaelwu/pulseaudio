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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <limits.h>
#include <asoundlib.h>

#ifdef HAVE_VALGRIND_MEMCHECK_H
#include <valgrind/memcheck.h>
#endif

#include <pulse/sample.h>
#include <pulse/xmalloc.h>
#include <pulse/timeval.h>
#include <pulse/util.h>

#include <pulsecore/log.h>
#include <pulsecore/macro.h>
#include <pulsecore/core-util.h>
#include <pulsecore/atomic.h>
#include <pulsecore/core-error.h>
#include <pulsecore/once.h>
#include <pulsecore/thread.h>
#include <pulsecore/conf-parser.h>
#include <pulsecore/strbuf.h>

#include "alsa-mixer.h"
#include "alsa-util.h"
#include "alsa-ucm.h"

#define PA_UCM_PLAYBACK_PRIORITY_UNSET(device)      ((device)->playback_channels && !(device)->playback_priority)
#define PA_UCM_CAPTURE_PRIORITY_UNSET(device)       ((device)->capture_channels && !(device)->capture_priority)
#define PA_UCM_DEVICE_PRIORITY_SET(device, priority) \
    do { \
        if (PA_UCM_PLAYBACK_PRIORITY_UNSET(device)) (device)->playback_priority = (priority);   \
        if (PA_UCM_CAPTURE_PRIORITY_UNSET(device))  (device)->capture_priority = (priority);    \
    } while(0)

struct ucm_items {
    const char *id;
    const char *property;
};

struct ucm_info {
    const char *id;
    unsigned priority;
};

static struct ucm_items item[] = {
    {"PlaybackPCM", PA_PROP_UCM_SINK},
    {"CapturePCM", PA_PROP_UCM_SOURCE},
    {"PlaybackVolume", PA_PROP_UCM_PLAYBACK_VOLUME},
    {"PlaybackSwitch", PA_PROP_UCM_PLAYBACK_SWITCH},
    {"PlaybackPriority", PA_PROP_UCM_PLAYBACK_PRIORITY},
    {"PlaybackChannels", PA_PROP_UCM_PLAYBACK_CHANNELS},
    {"CaptureVolume", PA_PROP_UCM_CAPTURE_VOLUME},
    {"CaptureSwitch", PA_PROP_UCM_CAPTURE_SWITCH},
    {"CapturePriority", PA_PROP_UCM_CAPTURE_PRIORITY},
    {"CaptureChannels", PA_PROP_UCM_CAPTURE_CHANNELS},
    {"TQ", PA_PROP_UCM_QOS},
    {NULL, NULL},
};

/* UCM verb info - this should eventually be part of policy manangement */
static struct ucm_info verb_info[] = {
    {SND_USE_CASE_VERB_INACTIVE, 0},
    {SND_USE_CASE_VERB_HIFI, 8000},
    {SND_USE_CASE_VERB_HIFI_LOW_POWER, 7000},
    {SND_USE_CASE_VERB_VOICE, 6000},
    {SND_USE_CASE_VERB_VOICE_LOW_POWER, 5000},
    {SND_USE_CASE_VERB_VOICECALL, 4000},
    {SND_USE_CASE_VERB_IP_VOICECALL, 4000},
    {SND_USE_CASE_VERB_ANALOG_RADIO, 3000},
    {SND_USE_CASE_VERB_DIGITAL_RADIO, 3000},
    {NULL, 0}
};

/* UCM device info - should be overwritten by ucm property */
static struct ucm_info dev_info[] = {
    {SND_USE_CASE_DEV_SPEAKER, 100},
    {SND_USE_CASE_DEV_LINE, 100},
    {SND_USE_CASE_DEV_HEADPHONES, 100},
    {SND_USE_CASE_DEV_HEADSET, 300},
    {SND_USE_CASE_DEV_HANDSET, 200},
    {SND_USE_CASE_DEV_BLUETOOTH, 400},
    {SND_USE_CASE_DEV_EARPIECE, 100},
    {SND_USE_CASE_DEV_SPDIF, 100},
    {SND_USE_CASE_DEV_HDMI, 100},
    {SND_USE_CASE_DEV_NONE, 100},
    {NULL, 0}
};

/* UCM profile properties - The verb data is store so it can be used to fill
 * the new profiles properties */
static int ucm_get_property(struct pa_alsa_ucm_verb *verb, snd_use_case_mgr_t *uc_mgr, const char *verb_name) {
    const char *value;
    char *id;
    int i = 0;

    do {
        int err;

        id = pa_sprintf_malloc("=%s//%s", item[i].id, verb_name);
        err = snd_use_case_get(uc_mgr, id, &value);
        pa_xfree(id);
        if (err < 0 ) {
            pa_log_info("No %s for verb %s", item[i].id, verb_name);
            continue;
        }

        pa_log_info("Got %s for verb %s: %s", item[i].id, verb_name, value);
        pa_proplist_sets(verb->proplist, item[i].property, value);
        free((void*)value);
    } while (item[++i].id);

    return 0;
};

static char **dup_strv(const char **src, int n) {
    char **dest = pa_xnew0(char *, n+1);
    int i;
    for (i=0; i<n; i++) {
        dest[i] = pa_xstrdup(src[i]);
    }
    return dest;
}

static int ucm_device_in(char **device_names, int num, pa_alsa_ucm_device *dev) {
    int i;
    const char *dev_name = pa_proplist_gets(dev->proplist, PA_PROP_UCM_NAME);

    for (i=0; i<num; i++)
    {
        if (!strcmp(dev_name, device_names[i]))
            return 1;
    }

    return 0;
}

/* Create a property list for this ucm device */
static int ucm_get_device_property(struct pa_alsa_ucm_device *device,
        snd_use_case_mgr_t *uc_mgr, struct pa_alsa_ucm_verb *verb, const char *device_name) {
    const char *value;
    const char **devices;
    char *id;
    int i = 0;
    int err;
    uint32_t ui;

    do {
        id = pa_sprintf_malloc("=%s/%s", item[i].id, device_name);
        err = snd_use_case_get(uc_mgr, id, &value);
        pa_xfree(id);
        if (err < 0) {
            pa_log_info("No %s for device %s", item[i].id, device_name);
            continue;
        }

        pa_log_info("Got %s for device %s: %s", item[i].id, device_name, value);
        pa_proplist_sets(device->proplist, item[i].property, value);
        free((void*)value);
    }  while (item[++i].id);

    /* get direction and channels */
    value = pa_proplist_gets(device->proplist, PA_PROP_UCM_PLAYBACK_CHANNELS);
    if (value) { /* output */
        /* get channels */
        if (pa_atou(value, &ui) == 0 && ui < PA_CHANNELS_MAX)
            device->playback_channels = ui;
        else
            pa_log("UCM playback channels %s for device %s out of range", value, device_name);

        /* get pcm */
        value = pa_proplist_gets(device->proplist, PA_PROP_UCM_SINK);
        if (!value) { /* take pcm from verb playback default */
            value = pa_proplist_gets(verb->proplist, PA_PROP_UCM_SINK);
            if (value) {
                pa_log_info("UCM playback device %s fetch pcm from verb default %s", device_name, value);
                pa_proplist_sets(device->proplist, PA_PROP_UCM_SINK, value);
            }
            else {
                pa_log("UCM playback device %s fetch pcm failed", device_name);
            }
        }
    }
    value = pa_proplist_gets(device->proplist, PA_PROP_UCM_CAPTURE_CHANNELS);
    if (value) { /* input */
        /* get channels */
        if (pa_atou(value, &ui) == 0 && ui < PA_CHANNELS_MAX)
            device->capture_channels = ui;
        else
            pa_log("UCM capture channels %s for device %s out of range", value, device_name);

        /* get pcm */
        value = pa_proplist_gets(device->proplist, PA_PROP_UCM_SOURCE);
        if (!value) { /* take pcm from verb capture default */
            value = pa_proplist_gets(verb->proplist, PA_PROP_UCM_SOURCE);
            if (value) {
                pa_log_info("UCM capture device %s fetch pcm from verb default %s", device_name, value);
                pa_proplist_sets(device->proplist, PA_PROP_UCM_SOURCE, value);
            }
            else {
                pa_log("UCM capture device %s fetch pcm failed", device_name);
            }
        }
    }
    pa_assert(device->playback_channels || device->capture_channels);

    value = pa_proplist_gets(device->proplist, PA_PROP_UCM_PLAYBACK_VOLUME);
    if (value) {
        device->playback_volume = pa_xstrdup(value);
    }

    /* get priority of device */
    if (device->playback_channels) { /* sink device */
        value = pa_proplist_gets(device->proplist, PA_PROP_UCM_PLAYBACK_PRIORITY);
        if (value) {
            /* get priority from ucm config */
            if (pa_atou(value, &ui) == 0)
                device->playback_priority = ui;
            else
                pa_log("UCM playback priority %s for device %s error", value, device_name);
        }
    }
    if (device->capture_channels) { /* source device */
        value = pa_proplist_gets(device->proplist, PA_PROP_UCM_CAPTURE_PRIORITY);
        if (value) {
            /* get priority from ucm config */
            if (pa_atou(value, &ui) == 0)
                device->capture_priority = ui;
            else
                pa_log("UCM capture priority %s for device %s error", value, device_name);
        }
    }
    if (PA_UCM_PLAYBACK_PRIORITY_UNSET(device) || PA_UCM_CAPTURE_PRIORITY_UNSET(device)) {
        /* get priority from static table */
        i = 0;
        do {
            if (strcasecmp(dev_info[i].id, device_name) == 0) {
                PA_UCM_DEVICE_PRIORITY_SET(device, dev_info[i].priority);
                break;
            }
        } while (dev_info[++i].id);
    }
    if (PA_UCM_PLAYBACK_PRIORITY_UNSET(device)) {
        /* fall through to default priority */
        device->playback_priority = 100;
    }
    if (PA_UCM_CAPTURE_PRIORITY_UNSET(device)) {
        /* fall through to default priority */
        device->capture_priority = 100;
    }

    id = pa_sprintf_malloc("%s/%s", "_conflictingdevs", device_name);
    device->n_confdev = snd_use_case_get_list(uc_mgr, id, &devices);
    pa_xfree(id);
    if (device->n_confdev <= 0)
        pa_log_info("No %s for device %s", "_conflictingdevs", device_name);
    else {
        device->conflicting_devices = dup_strv(devices, device->n_confdev);
        snd_use_case_free_list(devices, device->n_confdev);
    }

    id = pa_sprintf_malloc("%s/%s", "_supporteddevs", device_name);
    device->n_suppdev = snd_use_case_get_list(uc_mgr, id, &devices);
    pa_xfree(id);
    if (device->n_suppdev <= 0)
        pa_log_info("No %s for device %s", "_supporteddevs", device_name);
    else {
        device->supported_devices = dup_strv(devices, device->n_suppdev);
        snd_use_case_free_list(devices, device->n_suppdev);
    }

    return 0;
};

/* Create a property list for this ucm modifier */
static int ucm_get_modifier_property(struct pa_alsa_ucm_modifier *modifier, snd_use_case_mgr_t *uc_mgr, const char *modifier_name) {
    const char *value;
    char *id;
    int i = 0;

    do {
        int err;

        id = pa_sprintf_malloc("=%s/%s", item[i].id, modifier_name);
        err = snd_use_case_get(uc_mgr, id, &value);
        pa_xfree(id);
        if (err < 0 ) {
            pa_log_info("No %s for modifier %s", item[i].id, modifier_name);
            continue;
        }

        pa_log_info("Got %s for modifier %s: %s", item[i].id, modifier_name, value);
        pa_proplist_sets(modifier->proplist, item[i].property, value);
        free((void*)value);
    } while (item[++i].id);

    id = pa_sprintf_malloc("%s/%s", "_conflictingdevs", modifier_name);
    modifier->n_confdev = snd_use_case_get_list(uc_mgr, id, &modifier->conflicting_devices);
    pa_xfree(id);
    if (modifier->n_confdev < 0)
        pa_log_info("No %s for modifier %s", "_conflictingdevs", modifier_name);

    id = pa_sprintf_malloc("%s/%s", "_supporteddevs", modifier_name);
    modifier->n_suppdev = snd_use_case_get_list(uc_mgr, id, &modifier->supported_devices);
    pa_xfree(id);
    if (modifier->n_suppdev < 0)
        pa_log_info("No %s for modifier %s", "_supporteddevs", modifier_name);

    return 0;
};

/* Create a list of devices for this verb */
static int ucm_get_devices(struct pa_alsa_ucm_verb *verb, snd_use_case_mgr_t *uc_mgr) {
    const char **dev_list;
    int num_dev, i;

    num_dev = snd_use_case_get_list(uc_mgr, "_devices", &dev_list);
    if (num_dev <= 0)
        return num_dev;

    for (i = 0; i < num_dev; i += 2) {
        pa_alsa_ucm_device *d;
        d = pa_xnew0(pa_alsa_ucm_device, 1);
        d->proplist = pa_proplist_new();
        pa_proplist_sets(d->proplist, PA_PROP_UCM_NAME, pa_strnull(dev_list[i]));
        pa_proplist_sets(d->proplist, PA_PROP_UCM_DESCRIPTION, pa_strna(dev_list[i+1]));
        PA_LLIST_PREPEND(pa_alsa_ucm_device, verb->devices, d);
    }
    snd_use_case_free_list(dev_list, num_dev);

    return 0;
};

static int ucm_get_modifiers(struct pa_alsa_ucm_verb *verb, snd_use_case_mgr_t *uc_mgr) {
    const char **mod_list;
    int num_mod, i;

    num_mod = snd_use_case_get_list(uc_mgr, "_modifiers", &mod_list);
    if (num_mod <= 0)
        return num_mod;

    for (i = 0; i < num_mod; i += 2) {
        pa_alsa_ucm_modifier *m;
        m = pa_xnew0(pa_alsa_ucm_modifier, 1);
        m->proplist = pa_proplist_new();
        pa_proplist_sets(m->proplist, PA_PROP_UCM_NAME, pa_strnull(mod_list[i]));
        pa_proplist_sets(m->proplist, PA_PROP_UCM_DESCRIPTION, pa_strna(mod_list[i+1]));
        PA_LLIST_PREPEND(pa_alsa_ucm_modifier, verb->modifiers, m);
    }
    snd_use_case_free_list(mod_list, num_mod);

    return 0;
};

static pa_bool_t role_match(const char *cur, const char *role) {
    char *r;
    const char *state=NULL;

    if (!cur || !role)
        return FALSE;

    while ((r = pa_split_spaces(cur, &state))) {
        if (!strcasecmp(role, r)) {
            pa_xfree(r);
            return TRUE;
        }
        pa_xfree(r);
    }

    return FALSE;
}

static void add_role_to_device (pa_alsa_ucm_device *dev, const char *dev_name,
        const char *role_name, const char *role) {
    const char *cur = pa_proplist_gets(dev->proplist, role_name);

    if (!cur) {
        pa_proplist_sets(dev->proplist, role_name, role);
    }
    else if (!role_match(cur, role)) /* not exists */
    {
        char *value = pa_sprintf_malloc("%s %s", cur, role);
        pa_proplist_sets(dev->proplist, role_name, value);
        pa_xfree(value);
    }
    pa_log_info("Add role %s to device %s(%s), result %s", role,
            dev_name, role_name, pa_proplist_gets(dev->proplist, role_name));
}

static void add_media_role (const char *name, pa_alsa_ucm_device *list,
        const char *role_name, const char *role, int is_sink) {
    pa_alsa_ucm_device *d;

    PA_LLIST_FOREACH(d, list) {
        const char *dev_name = pa_proplist_gets(d->proplist, PA_PROP_UCM_NAME);
        if (!strcmp(dev_name, name)) {
            const char *sink = pa_proplist_gets(d->proplist, PA_PROP_UCM_SINK);
            const char *source = pa_proplist_gets(d->proplist, PA_PROP_UCM_SOURCE);
            if (is_sink && sink)
                add_role_to_device (d, dev_name, role_name, role);
            else if (!is_sink && source)
                add_role_to_device (d, dev_name, role_name, role);
            break;
        }
    }
}

static void ucm_set_media_roles(struct pa_alsa_ucm_modifier *modifier,
        pa_alsa_ucm_device *list, const char *mod_name) {
    int i;
    int is_sink=0;
    const char *sub = NULL;
    const char *role_name;

    if (pa_startswith(mod_name, "Play")) {
        is_sink = 1;
        sub = mod_name + 4;
    }
    else if (pa_startswith(mod_name, "Capture")) {
        sub = mod_name + 7;
    }

    if (!sub || !*sub) {
        pa_log_warn("Can't match media roles for modifer %s", mod_name);
        return;
    }

    modifier->action_direct = is_sink ?
        PA_ALSA_UCM_DIRECT_SINK : PA_ALSA_UCM_DIRECT_SOURCE;
    modifier->media_role = pa_xstrdup(sub);

    role_name = is_sink ? PA_PROP_UCM_PLAYBACK_ROLES : PA_PROP_UCM_CAPTURE_ROLES;

    for (i=0; i<modifier->n_suppdev; i++)
    {
        add_media_role(modifier->supported_devices[i],
                list, role_name, sub, is_sink);
    }
}

static void append_me_to_device(pa_alsa_ucm_device *devices,
        const char *dev_name, pa_alsa_ucm_device *me, const char *my_name, int is_conflicting) {
    pa_alsa_ucm_device *d;
    char ***pdevices;
    int *pnum;
    PA_LLIST_FOREACH(d, devices) {
        const char *name = pa_proplist_gets(d->proplist, PA_PROP_UCM_NAME);
        if (!strcmp(name, dev_name)) {
            pdevices = is_conflicting ? &d->conflicting_devices : &d->supported_devices;
            pnum = is_conflicting ? &d->n_confdev : &d->n_suppdev;
            if (!ucm_device_in(*pdevices, *pnum, me)) {
                /* append my name */
                *pdevices = pa_xrealloc(*pdevices, sizeof(char *) * (*pnum+2));
                (*pdevices)[*pnum] = pa_xstrdup(my_name);
                (*pdevices)[*pnum+1] = NULL;
                (*pnum)++;
                pa_log_info("== Device %s complemented to %s's %s list",
                        my_name, name, is_conflicting ? "conflicting" : "supported");
            }
            break;
        }
    }
}

static void append_lost_relationship(pa_alsa_ucm_device *devices,
        pa_alsa_ucm_device *dev, const char *dev_name) {
    int i;
    for (i=0; i<dev->n_confdev; i++) {
        append_me_to_device(devices, dev->conflicting_devices[i], dev, dev_name, 1);
    }
    for (i=0; i<dev->n_suppdev; i++) {
        append_me_to_device(devices, dev->supported_devices[i], dev, dev_name, 0);
    }
}

int ucm_get_verb(snd_use_case_mgr_t *uc_mgr, const char *verb_name,
        const char *verb_desc, struct pa_alsa_ucm_verb **p_verb) {
    struct pa_alsa_ucm_device *d;
    struct pa_alsa_ucm_modifier *mod;
    struct pa_alsa_ucm_verb *verb;
    int err=0;

    *p_verb = NULL;
    pa_log_info("ucm_get_verb: Set ucm verb to %s", verb_name);
    err = snd_use_case_set(uc_mgr, "_verb", verb_name);
    if (err < 0)
        return err;

    verb = pa_xnew0(pa_alsa_ucm_verb, 1);
    verb->proplist = pa_proplist_new();
    pa_proplist_sets(verb->proplist, PA_PROP_UCM_NAME, pa_strnull(verb_name));
    pa_proplist_sets(verb->proplist, PA_PROP_UCM_DESCRIPTION, pa_strna(verb_desc));
    err = ucm_get_devices(verb, uc_mgr);
    if (err < 0)
        pa_log("No UCM devices for verb %s", verb_name);

    err = ucm_get_modifiers(verb, uc_mgr);
    if (err < 0)
        pa_log("No UCM modifiers for verb %s", verb_name);

    /* Verb properties */
    ucm_get_property(verb, uc_mgr, verb_name);

    PA_LLIST_FOREACH(d, verb->devices) {
        const char *dev_name = pa_proplist_gets(d->proplist, PA_PROP_UCM_NAME);

        /* Devices properties */
        ucm_get_device_property(d, uc_mgr, verb, dev_name);
    }
    /* make conflicting or supported device mutual */
    PA_LLIST_FOREACH(d, verb->devices) {
        const char *dev_name = pa_proplist_gets(d->proplist, PA_PROP_UCM_NAME);
        append_lost_relationship(verb->devices, d, dev_name);
    }

    PA_LLIST_FOREACH(mod, verb->modifiers) {
        const char *mod_name = pa_proplist_gets(mod->proplist, PA_PROP_UCM_NAME);

        /* Modifier properties */
        ucm_get_modifier_property(mod, uc_mgr, mod_name);

        /* Set PA_PROP_DEVICE_INTENDED_ROLES property to devices */
        pa_log_info("Set media roles for verb %s, modifier %s", verb_name, mod_name);
        ucm_set_media_roles(mod, verb->devices, mod_name);
    }

    *p_verb = verb;
    return 0;
}

static void ucm_add_port_combination(pa_hashmap *hash, pa_alsa_ucm_mapping_context *context,
        int is_sink, int *dev_indices, int num, pa_hashmap *ports, pa_card_profile *cp, pa_core *core) {
    pa_device_port *port;
    int i;
    unsigned priority;
    char *name, *desc;
    const char *dev_name;
    const char *direction;
    pa_alsa_ucm_device *dev;

    dev = context->ucm_devices[dev_indices[0]];
    dev_name = pa_proplist_gets(dev->proplist, PA_PROP_UCM_NAME);
    name = pa_xstrdup(dev_name);
    desc = num == 1 ? pa_xstrdup(pa_proplist_gets(dev->proplist, PA_PROP_UCM_DESCRIPTION))
            : pa_sprintf_malloc("Combination port for %s", dev_name);
    priority = is_sink ? dev->playback_priority : dev->capture_priority;
    for (i=1; i<num; i++)
    {
        char *tmp;
        dev = context->ucm_devices[dev_indices[i]];
        dev_name = pa_proplist_gets(dev->proplist, PA_PROP_UCM_NAME);
        tmp = pa_sprintf_malloc("%s+%s", name, dev_name);
        pa_xfree(name);
        name = tmp;
        tmp = pa_sprintf_malloc("%s,%s", desc, dev_name);
        pa_xfree(desc);
        desc = tmp;
        /* FIXME: Is it true? */
        priority += (is_sink ? dev->playback_priority : dev->capture_priority);
    }

    port = pa_hashmap_get(ports, name);
    if (!port) {
        port = pa_device_port_new(core, pa_strna(name), desc, sizeof(pa_alsa_port_data_ucm));
        pa_assert(port);
        pa_hashmap_put(ports, port->name, port);
        pa_log_debug("Add port %s: %s", port->name, port->description);
        port->profiles = pa_hashmap_new(pa_idxset_string_hash_func, pa_idxset_string_compare_func);
    }

    port->priority = priority;
    if (is_sink)
        port->is_output = TRUE;
    else
        port->is_input = TRUE;

    pa_xfree(name);
    pa_xfree(desc);

    direction = is_sink ? "output" : "input";
    pa_log_debug("Port %s direction %s, priority %d", port->name, direction, priority);

    if (cp) {
        pa_log_debug("Adding port %s to profile %s", port->name, cp->name);
        pa_hashmap_put(port->profiles, cp->name, cp);
    }
    if (hash) {
        pa_hashmap_put(hash, port->name, port);
        pa_device_port_ref(port);
    }
}

static int ucm_device_contain(pa_alsa_ucm_mapping_context *context,
        int *dev_indices, int dev_num, const char *device_name) {
    int i;
    const char *dev_name;
    pa_alsa_ucm_device *dev;

    for (i=0; i<dev_num; i++)
    {
        dev = context->ucm_devices[dev_indices[i]];
        dev_name = pa_proplist_gets(dev->proplist, PA_PROP_UCM_NAME);
        if (!strcmp(dev_name, device_name))
            return 1;
    }

    return 0;
}

static int ucm_port_contain(const char *port_name, const char *dev_name) {
    int ret=0;
    char *r;
    const char *state=NULL;

    if (!port_name || !dev_name)
        return FALSE;

    while ((r = pa_split(port_name, "+", &state))) {
        if (!strcmp(r, dev_name)) {
            pa_xfree(r);
            ret = 1;
            break;
        }
        pa_xfree(r);
    }
    return ret;
}

static int ucm_check_conformance(pa_alsa_ucm_mapping_context *context,
        int *dev_indices, int dev_num, int map_index) {
    int i;
    pa_alsa_ucm_device *dev = context->ucm_devices[map_index];

    pa_log_debug("Check device %s conformance with %d other devices",
            pa_proplist_gets(dev->proplist, PA_PROP_UCM_NAME), dev_num);
    if (dev_num == 0) {
        pa_log_debug("First device in combination, number 1");
        return 1;
    }

    if (dev->n_confdev > 0)
    { /* the device defines conflicting devices */
        for (i=0; i<dev->n_confdev; i++)
        {
            if (ucm_device_contain(context, dev_indices,
                        dev_num, dev->conflicting_devices[i])) {
                pa_log_debug("Conflicting device found");
                return 0;
            }
        }
    }
    else if (dev->n_suppdev >= dev_num)
    { /* the device defines supported devices */
        for (i=0; i<dev_num; i++)
        {
            if (!ucm_device_in(dev->supported_devices,
                dev->n_suppdev, context->ucm_devices[dev_indices[i]])) {
                pa_log_debug("Supported device not found");
                return 0;
            }
        }
    }
    else { /* not support any other devices */
        pa_log_debug("Not support any other devices");
        return 0;
    }

    pa_log_debug("Device added to combination, number %d", dev_num+1);
    return 1;
}

void ucm_add_ports_combination(pa_hashmap *hash,
        pa_alsa_ucm_mapping_context *context, int is_sink, int *dev_indices, int dev_num,
        int map_index, pa_hashmap *ports, pa_card_profile *cp, pa_core *core) {

    if (map_index >= context->ucm_devices_num)
        return;

    /* check if device at map_index can combine with existing devices combination */
    if (ucm_check_conformance(context, dev_indices, dev_num, map_index))
    {
        /* add device at map_index to devices combination */
        dev_indices[dev_num] = map_index;
        /* add current devices combination as a new port */
        ucm_add_port_combination(hash, context, is_sink, dev_indices, dev_num+1, ports, cp, core);
        /* try more elements combination */
        ucm_add_ports_combination(hash, context, is_sink, dev_indices, dev_num+1, map_index+1, ports, cp, core);
    }
    /* try other device with current elements number */
    ucm_add_ports_combination(hash, context, is_sink, dev_indices, dev_num, map_index+1, ports, cp, core);
}

static char* merge_roles(const char *cur, const char *add) {
    char *r, *ret = NULL;
    const char *state=NULL;

    if (add == NULL)
        return pa_xstrdup(cur);
    else if (cur == NULL)
        return pa_xstrdup(add);

    while ((r = pa_split_spaces(add, &state))) {
        char *value;
        if (!ret)
            value = pa_xstrdup(r);
        else if (!role_match (cur, r))
            value = pa_sprintf_malloc("%s %s", ret, r);
        else {
            pa_xfree(r);
            continue;
        }
        pa_xfree(ret);
        ret = value;
        pa_xfree(r);
    }

    return ret;
}

void ucm_add_ports(pa_hashmap **p, pa_proplist *proplist,
        pa_alsa_ucm_mapping_context *context, int is_sink, pa_card *card) {
    int *dev_indices = pa_xnew(int, context->ucm_devices_num);
    int i;
    char *merged_roles;
    const char *role_name = is_sink ? PA_PROP_UCM_PLAYBACK_ROLES : PA_PROP_UCM_CAPTURE_ROLES;

    pa_assert(p);
    pa_assert(!*p);
    pa_assert(context->ucm_devices_num > 0);

    /* add ports first */
    *p = pa_hashmap_new(pa_idxset_string_hash_func, pa_idxset_string_compare_func);
    ucm_add_ports_combination(*p, context, is_sink, dev_indices, 0, 0, card->ports, NULL, card->core);
    pa_xfree(dev_indices);

    /* then set property PA_PROP_DEVICE_INTENDED_ROLES */
    merged_roles = pa_xstrdup(pa_proplist_gets(proplist, PA_PROP_DEVICE_INTENDED_ROLES));
    for (i=0; i<context->ucm_devices_num; i++)
    {
        const char *roles = pa_proplist_gets(
                context->ucm_devices[i]->proplist, role_name);
        char *tmp;
        tmp = merge_roles(merged_roles, roles);
        pa_xfree(merged_roles);
        merged_roles = tmp;
    }
    if (merged_roles) {
        pa_proplist_sets(proplist, PA_PROP_DEVICE_INTENDED_ROLES, merged_roles);
    }
    pa_log_info("Alsa device %s roles: %s", pa_proplist_gets(
                proplist, PA_PROP_DEVICE_STRING), pa_strnull(merged_roles));
    pa_xfree(merged_roles);
}

/* Change UCM verb and device to match selected card profile */
int ucm_set_profile(struct pa_alsa_ucm_config *ucm, const char *new_profile,
        const char *old_profile) {
    int ret = 0;
    const char *profile;
    pa_alsa_ucm_verb *verb;

    if (new_profile == old_profile)
        return ret;
    else if (new_profile == NULL || old_profile == NULL)
        profile = new_profile ? new_profile : SND_USE_CASE_VERB_INACTIVE;
    else if (strcmp(new_profile, old_profile) != 0)
        profile = new_profile;
    else
        return ret;

    /* change verb */
    pa_log_info("Set ucm verb to %s", profile);
    if ((snd_use_case_set(ucm->ucm_mgr, "_verb", profile)) < 0) {
        pa_log("failed to set verb %s", profile);
        ret = -1;
    }

    /* find active verb */
    ucm->active_verb = NULL;
    PA_LLIST_FOREACH(verb, ucm->verbs) {
        const char *verb_name;
        verb_name = pa_proplist_gets(verb->proplist, PA_PROP_UCM_NAME);
        if (!strcmp(verb_name, profile)) {
            ucm->active_verb = verb;
            break;
        }
    }

    return ret;
}

int ucm_set_port(pa_alsa_ucm_mapping_context *context, pa_device_port *port, int is_sink) {
    int i, ret=0;
    pa_alsa_ucm_config *ucm;
    char *enable_devs=NULL;
    char *r;
    const char *state=NULL;

    pa_assert(context && context->ucm);

    ucm = context->ucm;
    pa_assert(ucm->ucm_mgr);

    /* first disable then enable */
    for (i=0; i<context->ucm_devices_num; i++)
    {
        const char *dev_name = pa_proplist_gets(context->ucm_devices[i]->proplist, PA_PROP_UCM_NAME);
        if (ucm_port_contain(port->name, dev_name))
        {
            char *tmp = enable_devs ? pa_sprintf_malloc("%s,%s",
                    enable_devs, dev_name) : pa_xstrdup(dev_name);
            pa_xfree(enable_devs);
            enable_devs = tmp;
        }
        else
        {
            pa_log_info("Disable ucm device %s", dev_name);
            if (snd_use_case_set(ucm->ucm_mgr, "_disdev", dev_name) > 0) {
                pa_log("failed to disable ucm device %s", dev_name);
                ret = -1;
                break;
            }
        }
    }
    if (enable_devs) {
        while ((r = pa_split(enable_devs, ",", &state))) {
            pa_log_info("Enable ucm device %s", r);
            if (snd_use_case_set(ucm->ucm_mgr, "_enadev", r) < 0) {
                pa_log("failed to enable ucm device %s", r);
                pa_xfree(r);
                ret = -1;
                break;
            }
            pa_xfree(r);
        }
        pa_xfree(enable_devs);
    }
    return ret;
}

static void ucm_add_mapping(pa_alsa_profile *p, pa_alsa_mapping *m) {
    switch (m->direction) {
    case PA_ALSA_DIRECTION_ANY:
        pa_idxset_put(p->output_mappings, m, NULL);
        pa_idxset_put(p->input_mappings, m, NULL);
        break;
     case PA_ALSA_DIRECTION_OUTPUT:
        pa_idxset_put(p->output_mappings, m, NULL);
        break;
     case PA_ALSA_DIRECTION_INPUT:
        pa_idxset_put(p->input_mappings, m, NULL);
        break;
    }
}

static void alsa_mapping_add_ucm_device(pa_alsa_mapping *m, pa_alsa_ucm_device *device) {
    char *cur_desc;
    const char *new_desc;

    /* we expand 8 entries each time */
    if ((m->ucm_context.ucm_devices_num & 7) == 0)
        m->ucm_context.ucm_devices = pa_xrealloc(m->ucm_context.ucm_devices,
                sizeof(pa_alsa_ucm_device *) * (m->ucm_context.ucm_devices_num + 8));
    m->ucm_context.ucm_devices[m->ucm_context.ucm_devices_num++] = device;

    new_desc = pa_proplist_gets(device->proplist, PA_PROP_UCM_DESCRIPTION);
    cur_desc = m->description;
    if (cur_desc)
        m->description = pa_sprintf_malloc("%s + %s", cur_desc, new_desc);
    else
        m->description = pa_xstrdup(new_desc);
    pa_xfree(cur_desc);

    /* walk around null case */
    m->description = m->description ? m->description : pa_xstrdup("");

    /* save mapping to ucm device */
    if (m->direction == PA_ALSA_DIRECTION_OUTPUT)
        device->playback_mapping = m;
    else
        device->capture_mapping = m;
}

static int ucm_create_mapping_direction(struct pa_alsa_ucm_config *ucm,
        pa_alsa_profile_set *ps, struct pa_alsa_profile *p,
        struct pa_alsa_ucm_device *device, const char *verb_name,
        const char *device_name, const char *device_str, int is_sink) {
    pa_alsa_mapping *m;
    char *mapping_name;
    unsigned priority, channels;

    mapping_name = pa_sprintf_malloc("Mapping %s: %s: %s", verb_name, device_str, is_sink ? "sink" : "source");

    m = pa_alsa_mapping_get(ps, mapping_name);
    if (!m) {
        pa_log("no mapping for %s", mapping_name);
        pa_xfree(mapping_name);
        return -1;
    }
    pa_log_info("ucm mapping: %s dev %s", mapping_name, device_name);
    pa_xfree(mapping_name);

    priority = is_sink ? device->playback_priority : device->capture_priority;
    channels = is_sink ? device->playback_channels : device->capture_channels;
    if (m->ucm_context.ucm_devices_num == 0)
    {   /* new mapping */
        m->ucm_context.ucm = ucm;
        m->ucm_context.direction = is_sink ? PA_ALSA_UCM_DIRECT_SINK : PA_ALSA_UCM_DIRECT_SOURCE;

        m->device_strings = pa_xnew0(char*, 2);
        m->device_strings[0] = pa_xstrdup(device_str);
        m->direction = is_sink ? PA_ALSA_DIRECTION_OUTPUT : PA_ALSA_DIRECTION_INPUT;

        ucm_add_mapping(p, m);
        pa_channel_map_init_extend(&m->channel_map, channels, PA_CHANNEL_MAP_ALSA);
    }

    /* mapping priority is the highest one of ucm devices */
    if (priority > m->priority) {
        m->priority = priority;
    }
    /* mapping channels is the lowest one of ucm devices */
    if (channels < m->channel_map.channels) {
        pa_channel_map_init_extend(&m->channel_map, channels, PA_CHANNEL_MAP_ALSA);
    }
    alsa_mapping_add_ucm_device(m, device);

    return 0;
}

static int ucm_create_mapping(struct pa_alsa_ucm_config *ucm,
        pa_alsa_profile_set *ps, struct pa_alsa_profile *p,
        struct pa_alsa_ucm_device *device, const char *verb_name,
        const char *device_name, const char *sink, const char *source) {
    int ret=0;

    if (!sink && !source)
    {
        pa_log("no sink and source at %s: %s", verb_name, device_name);
        return -1;
    }

    if (sink)
        ret = ucm_create_mapping_direction(ucm, ps, p, device, verb_name, device_name, sink, 1);
    if (ret == 0 && source)
        ret = ucm_create_mapping_direction(ucm, ps, p, device, verb_name, device_name, source, 0);

    return ret;
}

static pa_alsa_jack* ucm_get_jack(struct pa_alsa_ucm_config *ucm, const char *dev_name) {
    pa_alsa_jack *j;

    PA_LLIST_FOREACH(j, ucm->jacks)
        if (pa_streq(j->name, dev_name))
            return j;

    j = pa_xnew0(pa_alsa_jack, 1);
    j->state_unplugged = PA_PORT_AVAILABLE_NO;
    j->state_plugged = PA_PORT_AVAILABLE_YES;
    j->name = pa_xstrdup(dev_name);
    j->alsa_name = pa_sprintf_malloc("%s Jack", dev_name);

    PA_LLIST_PREPEND(pa_alsa_jack, ucm->jacks, j);

    return j;
}

static int ucm_create_profile(struct pa_alsa_ucm_config *ucm, pa_alsa_profile_set *ps,
        struct pa_alsa_ucm_verb *verb, const char *verb_name, const char *verb_desc) {
    struct pa_alsa_profile *p;
    struct pa_alsa_ucm_device *dev;
    int i=0;

    pa_assert(ps);

    if (pa_hashmap_get(ps->profiles, verb_name)) {
        pa_log("verb %s already exists", verb_name);
        return -1;
    }

    p = pa_xnew0(pa_alsa_profile, 1);
    p->profile_set = ps;
    p->name = pa_xstrdup(verb_name);
    p->description = pa_xstrdup(verb_desc);

    p->output_mappings = pa_idxset_new(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func);
    p->input_mappings = pa_idxset_new(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func);

    p->supported = TRUE;
    pa_hashmap_put(ps->profiles, p->name, p);

    /* TODO: get profile priority from ucm info or policy management */
    do {
        /* We allow UCM verb name to be separated by "_",
         * while predefined alsa ucm name is splitted by " "
         */
        char *verb_cmp = pa_xstrdup(verb_name);
        char *c = verb_cmp;
        while (*c) {
            if (*c == '_') *c = ' ';
            c++;
        }
        if (strcasecmp(verb_info[i].id, verb_cmp) == 0)
        {
            p->priority = verb_info[i].priority;
            pa_xfree(verb_cmp);
            break;
        }
        pa_xfree(verb_cmp);
    } while (verb_info[++i].id);

    if (verb_info[++i].id == NULL)
        p->priority = 1000;

    PA_LLIST_FOREACH(dev, verb->devices) {
        const char *dev_name, *sink, *source;

        dev_name = pa_proplist_gets(dev->proplist, PA_PROP_UCM_NAME);

        sink = pa_proplist_gets(dev->proplist, PA_PROP_UCM_SINK);
        source = pa_proplist_gets(dev->proplist, PA_PROP_UCM_SOURCE);

        ucm_create_mapping(ucm, ps, p, dev, verb_name, dev_name, sink, source);
        dev->jack = ucm_get_jack(ucm, dev_name);
    }
    pa_alsa_profile_dump(p);

    return 0;
}

static snd_pcm_t* mapping_open_pcm(struct pa_alsa_ucm_config *ucm,
        pa_alsa_mapping *m, int mode) {
    pa_sample_spec try_ss = ucm->core->default_sample_spec;
    pa_channel_map try_map = m->channel_map;
    snd_pcm_uframes_t try_period_size, try_buffer_size;

    try_ss.channels = try_map.channels;

    try_period_size =
        pa_usec_to_bytes(ucm->core->default_fragment_size_msec * PA_USEC_PER_MSEC, &try_ss) /
        pa_frame_size(&try_ss);
    try_buffer_size = ucm->core->default_n_fragments * try_period_size;

    return pa_alsa_open_by_device_string(m->device_strings[0], NULL, &try_ss,
                              &try_map, mode, &try_period_size,
                              &try_buffer_size, 0, NULL, NULL, TRUE);
}

static void profile_finalize_probing(pa_alsa_profile *p) {
    pa_alsa_mapping *m;
    uint32_t idx;

    PA_IDXSET_FOREACH(m, p->output_mappings, idx) {
        if (!m->output_pcm)
            continue;

        if (p->supported)
            m->supported++;

        snd_pcm_close(m->output_pcm);
        m->output_pcm = NULL;
    }

    PA_IDXSET_FOREACH(m, p->input_mappings, idx) {
        if (!m->input_pcm)
            continue;

        if (p->supported)
            m->supported++;

        snd_pcm_close(m->input_pcm);
        m->input_pcm = NULL;
    }
}

static struct pa_alsa_ucm_device *find_ucm_dev(
        struct pa_alsa_ucm_verb *verb, const char *dev_name) {
    struct pa_alsa_ucm_device *dev;

    PA_LLIST_FOREACH(dev, verb->devices) {
        const char *name = pa_proplist_gets(dev->proplist, PA_PROP_UCM_NAME);
        if (pa_streq(name, dev_name))
            return dev;
    }

    return NULL;
}

static void ucm_mapping_jack_probe(pa_alsa_mapping *m) {
    snd_pcm_t *pcm_handle;
    snd_mixer_t *mixer_handle;
    snd_hctl_t *hctl_handle;
    pa_alsa_ucm_mapping_context *context = &m->ucm_context;
    struct pa_alsa_ucm_device *dev;
    int i;

    pcm_handle = m->direction == PA_ALSA_DIRECTION_OUTPUT ? m->output_pcm : m->input_pcm;
    mixer_handle = pa_alsa_open_mixer_for_pcm(pcm_handle, NULL, &hctl_handle);
    if (!mixer_handle || !hctl_handle)
        return;

    for (i=0; i<context->ucm_devices_num; i++) {
        dev = context->ucm_devices[i];
        pa_assert (dev->jack);
        dev->jack->has_control = pa_alsa_find_jack(hctl_handle, dev->jack->alsa_name) != NULL;
        pa_log_info("ucm_mapping_jack_probe: %s has_control=%d", dev->jack->name, dev->jack->has_control);
    }

    snd_mixer_close(mixer_handle);
}

static void ucm_probe_jacks(struct pa_alsa_ucm_config *ucm, pa_alsa_profile_set *ps) {
    void *state;
    pa_alsa_profile *p;
    pa_alsa_mapping *m;
    uint32_t idx;

    PA_HASHMAP_FOREACH(p, ps->profiles, state) {
        /* change verb */
        pa_log_info("ucm_probe_jacks: set ucm verb to %s", p->name);
        if ((snd_use_case_set(ucm->ucm_mgr, "_verb", p->name)) < 0) {
            pa_log("ucm_probe_jacks: failed to set verb %s", p->name);
            p->supported = FALSE;
            continue;
        }
        PA_IDXSET_FOREACH(m, p->output_mappings, idx) {
            m->output_pcm = mapping_open_pcm(ucm, m, SND_PCM_STREAM_PLAYBACK);
            if (!m->output_pcm) {
                p->supported = FALSE;
                break;
            }
        }
        if (p->supported) {
            PA_IDXSET_FOREACH(m, p->input_mappings, idx) {
                m->input_pcm = mapping_open_pcm(ucm, m, SND_PCM_STREAM_CAPTURE);
                if (!m->input_pcm) {
                    p->supported = FALSE;
                    break;
                }
            }
        }
        if (!p->supported) {
            profile_finalize_probing(p);
            continue;
        }

        pa_log_debug("Profile %s supported.", p->name);

        PA_IDXSET_FOREACH(m, p->output_mappings, idx)
            ucm_mapping_jack_probe(m);

        PA_IDXSET_FOREACH(m, p->input_mappings, idx)
            ucm_mapping_jack_probe(m);

        profile_finalize_probing(p);
    }

    /* restore ucm state */
    snd_use_case_set(ucm->ucm_mgr, "_verb", SND_USE_CASE_VERB_INACTIVE);

    pa_alsa_profile_set_drop_unsupported(ps);
}

pa_alsa_profile_set* ucm_add_profile_set(struct pa_alsa_ucm_config *ucm, pa_channel_map *default_channel_map) {
    struct pa_alsa_ucm_verb *verb;
    pa_alsa_profile_set *ps;

    ps = pa_xnew0(pa_alsa_profile_set, 1);
    ps->mappings = pa_hashmap_new(pa_idxset_string_hash_func, pa_idxset_string_compare_func);
    ps->profiles = pa_hashmap_new(pa_idxset_string_hash_func, pa_idxset_string_compare_func);
    ps->decibel_fixes = pa_hashmap_new(pa_idxset_string_hash_func, pa_idxset_string_compare_func);

    /* create a profile for each verb */
    PA_LLIST_FOREACH(verb, ucm->verbs) {
        const char *verb_name;
        const char *verb_desc;

        verb_name = pa_proplist_gets(verb->proplist, PA_PROP_UCM_NAME);
        verb_desc = pa_proplist_gets(verb->proplist, PA_PROP_UCM_DESCRIPTION);
        if (verb_name == NULL) {
            pa_log("verb with no name");
            continue;
        }

	    ucm_create_profile(ucm, ps, verb, verb_name, verb_desc);
    }

    ucm_probe_jacks(ucm, ps);
    ps->probed = TRUE;

    return ps;
}

static void free_verb(struct pa_alsa_ucm_verb *verb) {
    struct pa_alsa_ucm_device *di, *dn;
    struct pa_alsa_ucm_modifier *mi, *mn;

    PA_LLIST_FOREACH_SAFE(di, dn, verb->devices) {
        PA_LLIST_REMOVE(pa_alsa_ucm_device, verb->devices, di);
        pa_proplist_free(di->proplist);
        if (di->n_suppdev > 0)
            pa_xstrfreev(di->supported_devices);
        if (di->n_confdev > 0)
            pa_xstrfreev(di->conflicting_devices);
        pa_xfree(di);
    }

    PA_LLIST_FOREACH_SAFE(mi, mn, verb->modifiers) {
        PA_LLIST_REMOVE(pa_alsa_ucm_modifier, verb->modifiers, mi);
        pa_proplist_free(mi->proplist);
        if (mi->n_suppdev > 0)
            snd_use_case_free_list(mi->supported_devices, mi->n_suppdev);
        if (mi->n_confdev > 0)
            snd_use_case_free_list(mi->conflicting_devices, mi->n_confdev);
        pa_xfree(mi->media_role);
        pa_xfree(mi);
    }
    pa_proplist_free(verb->proplist);
    pa_xfree(verb);
}

void ucm_free(struct pa_alsa_ucm_config *ucm) {
    struct pa_alsa_ucm_verb *vi, *vn;
    pa_alsa_jack *ji, *jn;

    PA_LLIST_FOREACH_SAFE(vi, vn, ucm->verbs) {
        PA_LLIST_REMOVE(pa_alsa_ucm_verb, ucm->verbs, vi);
        free_verb(vi);
    }
    PA_LLIST_FOREACH_SAFE(ji, jn, ucm->jacks) {
        PA_LLIST_REMOVE(pa_alsa_jack, ucm->jacks, ji);
        pa_xfree(ji->alsa_name);
        pa_xfree(ji->name);
        pa_xfree(ji);
    }
    if (ucm->ucm_mgr)
    {
        snd_use_case_mgr_close(ucm->ucm_mgr);
        ucm->ucm_mgr = NULL;
    }
}

void ucm_mapping_context_free(pa_alsa_ucm_mapping_context *context) {
    struct pa_alsa_ucm_device *dev;
    int i;

    /* clear ucm device pointer to mapping */
    for (i=0; i<context->ucm_devices_num; i++) {
        dev = context->ucm_devices[i];
        if (context->direction == PA_ALSA_UCM_DIRECT_SINK)
            dev->playback_mapping = NULL;
        else
            dev->capture_mapping = NULL;
    }

    pa_xfree(context->ucm_devices);
}

static pa_bool_t stream_routed_to_mod_intent (struct pa_alsa_ucm_verb *verb,
        struct pa_alsa_ucm_modifier *mod, const char *mapping_name) {
    int i;
    const char *dev_name;
    struct pa_alsa_ucm_device *dev;
    pa_alsa_mapping *mapping;

    /* check if mapping_name is same as one of the modifier's supported device */
    for (i=0; i<mod->n_suppdev; i++) {
        dev_name = mod->supported_devices[i];
        /* first find the supported device */
        dev = find_ucm_dev(verb, dev_name);
        if (dev) {
            /* then match the mapping name */
            mapping = mod->action_direct == PA_ALSA_UCM_DIRECT_SINK ? dev->playback_mapping : dev->capture_mapping;
            if (mapping && pa_streq(mapping->name, mapping_name))
                return TRUE;
        }
    }

    return FALSE;
}

/* enable the modifier when both of the conditions are met
 * 1. the first stream with matched role starts
 * 2. the stream is routed to the device of the modifier specifies.
 */
void ucm_roled_stream_begin(pa_alsa_ucm_config *ucm,
        const char *role, const char *mapping_name, int is_sink) {
    struct pa_alsa_ucm_modifier *mod;

    if (!ucm->active_verb)
        return;

    PA_LLIST_FOREACH(mod, ucm->active_verb->modifiers) {
        if (((mod->action_direct == PA_ALSA_UCM_DIRECT_SINK && is_sink) ||
            (mod->action_direct == PA_ALSA_UCM_DIRECT_SOURCE && !is_sink)) &&
            (!strcasecmp(mod->media_role, role))) {
            if (stream_routed_to_mod_intent(ucm->active_verb, mod, mapping_name)) {
                if (mod->enabled_counter == 0) {
                    const char *mod_name = pa_proplist_gets(mod->proplist, PA_PROP_UCM_NAME);
                    pa_log_info("Enable ucm modifiers %s", mod_name);
                    if (snd_use_case_set(ucm->ucm_mgr, "_enamod", mod_name) < 0) {
                        pa_log("failed to enable ucm modifier %s", mod_name);
                    }
                }
                mod->enabled_counter++;
                //TODO: set port of the sink/source to modifier's intent device? */
            }
            break;
        }
    }
}

/* disable the modifier when both of the conditions are met
 * 1. the last stream with matched role ends
 * 2. the stream is routed to the device of the modifier specifies.
 */
void ucm_roled_stream_end(pa_alsa_ucm_config *ucm,
        const char *role, const char *mapping_name, int is_sink) {
    struct pa_alsa_ucm_modifier *mod;

    if (!ucm->active_verb)
        return;

    PA_LLIST_FOREACH(mod, ucm->active_verb->modifiers) {
        if (((mod->action_direct == PA_ALSA_UCM_DIRECT_SINK && is_sink) ||
            (mod->action_direct == PA_ALSA_UCM_DIRECT_SOURCE && !is_sink)) &&
            (!strcasecmp(mod->media_role, role))) {
            if (stream_routed_to_mod_intent(ucm->active_verb, mod, mapping_name)) {
                mod->enabled_counter--;
                if (mod->enabled_counter == 0) {
                    const char *mod_name = pa_proplist_gets(mod->proplist, PA_PROP_UCM_NAME);
                    pa_log_info("Disable ucm modifiers %s", mod_name);
                    if (snd_use_case_set(ucm->ucm_mgr, "_dismod", mod_name) < 0) {
                        pa_log("failed to disable ucm modifier %s", mod_name);
                    }
                }
                //TODO: set port of the sink/source to modifier's intent device? */
            }
            break;
        }
    }
}

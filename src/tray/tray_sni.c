#include "tray_sni.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <string.h>

#define WATCHER "org.kde.StatusNotifierWatcher"
#define WATCHER_PATH "/StatusNotifierWatcher"
#define ITEM "org.kde.StatusNotifierItem"
#define ITEM_PATH "/StatusNotifierItem"
#define MENU "com.canonical.dbusmenu"
#define MENU_PATH "/Menu"

static const char interfaces[] =
    "<node><interface name='org.kde.StatusNotifierItem'>"
    "<method name='Activate'><arg type='i' direction='in'/><arg type='i' direction='in'/></method>"
    "<method name='SecondaryActivate'><arg type='i' direction='in'/><arg type='i' direction='in'/></method>"
    "<method name='ContextMenu'><arg type='i' direction='in'/><arg type='i' direction='in'/></method>"
    "<method name='Scroll'><arg type='i' direction='in'/><arg type='s' direction='in'/></method>"
    "<property name='Category' type='s' access='read'/>"
    "<property name='Id' type='s' access='read'/>"
    "<property name='Title' type='s' access='read'/>"
    "<property name='Status' type='s' access='read'/>"
    "<property name='WindowId' type='i' access='read'/>"
    "<property name='Menu' type='o' access='read'/>"
    "<property name='ItemIsMenu' type='b' access='read'/>"
    "<property name='IconName' type='s' access='read'/>"
    "<property name='IconPixmap' type='a(iiay)' access='read'/>"
    "<property name='IconThemePath' type='s' access='read'/>"
    "<property name='OverlayIconName' type='s' access='read'/>"
    "<property name='OverlayIconPixmap' type='a(iiay)' access='read'/>"
    "<property name='AttentionIconName' type='s' access='read'/>"
    "<property name='AttentionIconPixmap' type='a(iiay)' access='read'/>"
    "<property name='AttentionMovieName' type='s' access='read'/>"
    "<property name='ToolTip' type='(sa(iiay)ss)' access='read'/>"
    "<signal name='NewTitle'/><signal name='NewIcon'/><signal name='NewToolTip'/>"
    "<signal name='NewStatus'><arg type='s'/></signal>"
    "</interface><interface name='com.canonical.dbusmenu'>"
    "<method name='GetLayout'><arg type='i' direction='in'/><arg type='i' direction='in'/>"
    "<arg type='as' direction='in'/><arg type='u' direction='out'/><arg type='(ia{sv}av)' direction='out'/></method>"
    "<method name='GetGroupProperties'><arg type='ai' direction='in'/><arg type='as' direction='in'/>"
    "<arg type='a(ia{sv})' direction='out'/></method>"
    "<method name='GetProperty'><arg type='i' direction='in'/><arg type='s' direction='in'/>"
    "<arg type='v' direction='out'/></method>"
    "<method name='Event'><arg type='i' direction='in'/><arg type='s' direction='in'/>"
    "<arg type='v' direction='in'/><arg type='u' direction='in'/></method>"
    "<method name='EventGroup'><arg type='a(isvu)' direction='in'/><arg type='ai' direction='out'/></method>"
    "<method name='AboutToShow'><arg type='i' direction='in'/><arg type='b' direction='out'/></method>"
    "<method name='AboutToShowGroup'><arg type='ai' direction='in'/>"
    "<arg type='ai' direction='out'/><arg type='ai' direction='out'/></method>"
    "<property name='Version' type='u' access='read'/>"
    "<property name='TextDirection' type='s' access='read'/>"
    "<property name='Status' type='s' access='read'/>"
    "<property name='IconThemePath' type='as' access='read'/>"
    "<signal name='LayoutUpdated'><arg type='u'/><arg type='i'/></signal>"
    "<signal name='ItemsPropertiesUpdated'><arg type='a(ia{sv})'/><arg type='a(ias)'/></signal>"
    "</interface></node>";

struct tray_sni {
    unsigned references;
    bool alive;
    bool usable;
    bool availability_known;
    const tray_menu *menu;
    unsigned revision;
    uint64_t epoch;
    GDBusConnection *bus;
    GCancellable *cancel;
    GVariant *icons;
    guint item_id;
    guint menu_id;
    guint watch;
    guint signals;
    guint properties;
    char *owner;
    void (*activate)(void *, int);
    void (*available)(void *, bool);
    void *context;
};

typedef struct { tray_sni *sni; uint64_t epoch; } host_call;

static tray_sni *retain(tray_sni *sni)
{
    sni->references++;
    return sni;
}

static void release(void *data)
{
    tray_sni *sni = data;
    if (--sni->references)
        return;
    g_clear_object(&sni->bus);
    g_clear_object(&sni->cancel);
    g_clear_pointer(&sni->icons, g_variant_unref);
    g_free(sni->owner);
    g_free(sni);
}

static host_call *new_call(tray_sni *sni)
{
    host_call *call = g_new(host_call, 1);
    call->sni = retain(sni);
    call->epoch = sni->epoch;
    return call;
}

static void free_call(host_call *call)
{
    release(call->sni);
    g_free(call);
}

static bool current(host_call *call)
{
    return call->sni->alive && call->epoch == call->sni->epoch;
}

static void availability(tray_sni *sni, bool usable)
{
    if (sni->alive && (!sni->availability_known || sni->usable != usable)) {
        sni->availability_known = true;
        sni->usable = usable;
        sni->available(sni->context, usable);
    }
}

static GVariant *load_icons(const char *path, GError **error)
{
    const int sizes[] = {16, 22, 32, 48};
    GVariantBuilder icons;
    g_variant_builder_init(&icons, G_VARIANT_TYPE("a(iiay)"));
    for (size_t i = 0; i < G_N_ELEMENTS(sizes); i++) {
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(path, sizes[i], sizes[i], TRUE, error);
        if (!pixbuf) {
            g_variant_builder_clear(&icons);
            return NULL;
        }
        int width = gdk_pixbuf_get_width(pixbuf), height = gdk_pixbuf_get_height(pixbuf);
        int channels = gdk_pixbuf_get_n_channels(pixbuf), stride = gdk_pixbuf_get_rowstride(pixbuf);
        const guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
        guchar *argb = g_malloc((size_t)width * height * 4);
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                const guchar *source = pixels + y * stride + x * channels;
                guchar *destination = argb + (y * width + x) * 4;
                destination[0] = channels == 4 ? source[3] : 255;
                memcpy(destination + 1, source, 3);
            }
        }
        g_variant_builder_add(&icons, "(ii@ay)", width, height,
            g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, argb, (size_t)width * height * 4, 1));
        g_free(argb);
        g_object_unref(pixbuf);
    }
    return g_variant_ref_sink(g_variant_builder_end(&icons));
}

static GVariant *row_properties(const tray_menu_item *item)
{
    GVariantBuilder properties;
    g_variant_builder_init(&properties, G_VARIANT_TYPE_VARDICT);
    if (item) {
        /* DBusMenu uses underscore mnemonics; double literal underscores. */
        GString *label = g_string_new(NULL);
        for (const char *p = item->label; *p; p++) {
            g_string_append_c(label, *p);
            if (*p == '_')
                g_string_append_c(label, '_');
        }
        g_variant_builder_add(&properties, "{sv}", "label", g_variant_new_string(label->str));
        g_string_free(label, TRUE);
        g_variant_builder_add(&properties, "{sv}", "enabled", g_variant_new_boolean(item->enabled));
        g_variant_builder_add(&properties, "{sv}", "visible", g_variant_new_boolean(TRUE));
        if (item->id == TRAY_SEPARATOR)
            g_variant_builder_add(&properties, "{sv}", "type", g_variant_new_string("separator"));
    }
    return g_variant_builder_end(&properties);
}

static GVariant *filtered_properties(const tray_menu_item *item, GVariant *names)
{
    GVariant *all = g_variant_ref_sink(row_properties(item));
    if (!names || g_variant_n_children(names) == 0)
        return all;
    GVariantBuilder filtered;
    g_variant_builder_init(&filtered, G_VARIANT_TYPE_VARDICT);
    GVariantIter iter;
    const char *name;
    g_variant_iter_init(&iter, names);
    while (g_variant_iter_next(&iter, "&s", &name)) {
        GVariant *value = g_variant_lookup_value(all, name, NULL);
        if (value) {
            g_variant_builder_add(&filtered, "{sv}", name, value);
            g_variant_unref(value);
        }
    }
    g_variant_unref(all);
    return g_variant_ref_sink(g_variant_builder_end(&filtered));
}

static GVariant *layout(tray_sni *sni, int id, int depth, GVariant *names)
{
    GVariantBuilder children;
    g_variant_builder_init(&children, G_VARIANT_TYPE("av"));
    if (id == 0 && depth != 0) {
        for (size_t i = 0; i < G_N_ELEMENTS(sni->menu->items); i++)
            g_variant_builder_add(&children, "v", layout(sni, sni->menu->items[i].id, 0, names));
    }
    return g_variant_new("(i@a{sv}av)", id,
        filtered_properties(tray_menu_find(sni->menu, id), names), &children);
}

static bool event(tray_sni *sni, int id, const char *name)
{
    const tray_menu_item *item = tray_menu_find(sni->menu, id);
    if (!item)
        return false;
    if (item->enabled && strcmp(name, "clicked") == 0)
        sni->activate(sni->context, id);
    return true;
}

static void invalid(GDBusMethodInvocation *invocation)
{
    g_dbus_method_invocation_return_dbus_error(invocation,
        "com.canonical.dbusmenu.Error.InvalidMenuItem", "Unknown menu item or property");
}

static void method_called(GDBusConnection *bus, const char *sender, const char *path,
    const char *interface, const char *method, GVariant *parameters,
    GDBusMethodInvocation *invocation, void *data)
{
    tray_sni *sni = data;
    (void)bus; (void)sender; (void)path;
    if (strcmp(interface, ITEM) == 0) {
        if (strcmp(method, "Scroll") != 0)
            sni->activate(sni->context, 0);
        g_dbus_method_invocation_return_value(invocation, NULL);
    } else if (strcmp(method, "GetLayout") == 0) {
        int id, depth;
        GVariant *names;
        g_variant_get(parameters, "(ii@as)", &id, &depth, &names);
        if (id != 0 && !tray_menu_find(sni->menu, id))
            invalid(invocation);
        else
            g_dbus_method_invocation_return_value(invocation,
                g_variant_new("(u@(ia{sv}av))", sni->menu->revision, layout(sni, id, depth, names)));
        g_variant_unref(names);
    } else if (strcmp(method, "GetGroupProperties") == 0) {
        GVariant *ids, *names;
        g_variant_get(parameters, "(@ai@as)", &ids, &names);
        GVariantBuilder rows;
        g_variant_builder_init(&rows, G_VARIANT_TYPE("a(ia{sv})"));
        GVariantIter iter;
        int id;
        g_variant_iter_init(&iter, ids);
        while (g_variant_iter_next(&iter, "i", &id)) {
            const tray_menu_item *item = tray_menu_find(sni->menu, id);
            if (item || id == 0)
                g_variant_builder_add(&rows, "(i@a{sv})", id, filtered_properties(item, names));
        }
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(a(ia{sv}))", &rows));
        g_variant_unref(ids);
        g_variant_unref(names);
    } else if (strcmp(method, "GetProperty") == 0) {
        int id;
        const char *name;
        g_variant_get(parameters, "(i&s)", &id, &name);
        GVariant *properties = filtered_properties(tray_menu_find(sni->menu, id), NULL);
        GVariant *value = g_variant_lookup_value(properties, name, NULL);
        if (value) {
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(v)", value));
            g_variant_unref(value);
        } else {
            invalid(invocation);
        }
        g_variant_unref(properties);
    } else if (strcmp(method, "Event") == 0) {
        int id;
        guint timestamp;
        const char *name;
        GVariant *value;
        g_variant_get(parameters, "(i&svu)", &id, &name, &value, &timestamp);
        if (event(sni, id, name))
            g_dbus_method_invocation_return_value(invocation, NULL);
        else
            invalid(invocation);
        g_variant_unref(value);
    } else if (strcmp(method, "EventGroup") == 0) {
        GVariant *events;
        g_variant_get(parameters, "(@a(isvu))", &events);
        GVariantIter iter;
        GVariantBuilder errors;
        g_variant_builder_init(&errors, G_VARIANT_TYPE("ai"));
        g_variant_iter_init(&iter, events);
        int id;
        const char *name;
        guint timestamp;
        GVariant *value;
        while (g_variant_iter_next(&iter, "(i&svu)", &id, &name, &value, &timestamp)) {
            if (!event(sni, id, name))
                g_variant_builder_add(&errors, "i", id);
            g_variant_unref(value);
        }
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(ai)", &errors));
        g_variant_unref(events);
    } else if (strcmp(method, "AboutToShow") == 0) {
        int id;
        g_variant_get(parameters, "(i)", &id);
        if (id != 0 && !tray_menu_find(sni->menu, id))
            invalid(invocation);
        else
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", FALSE));
    } else if (strcmp(method, "AboutToShowGroup") == 0) {
        GVariant *ids;
        g_variant_get(parameters, "(@ai)", &ids);
        GVariantBuilder updates, errors;
        g_variant_builder_init(&updates, G_VARIANT_TYPE("ai"));
        g_variant_builder_init(&errors, G_VARIANT_TYPE("ai"));
        GVariantIter iter;
        int id;
        g_variant_iter_init(&iter, ids);
        while (g_variant_iter_next(&iter, "i", &id))
            if (id && !tray_menu_find(sni->menu, id))
                g_variant_builder_add(&errors, "i", id);
        g_variant_unref(ids);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(aiai)", &updates, &errors));
    }
}

static GVariant *get_property(GDBusConnection *bus, const char *sender, const char *path,
    const char *interface, const char *property, GError **error, void *data)
{
    tray_sni *sni = data;
    (void)bus; (void)sender; (void)path; (void)error;
    if (strcmp(interface, MENU) == 0) {
        if (strcmp(property, "Version") == 0) return g_variant_new_uint32(3);
        if (strcmp(property, "TextDirection") == 0) return g_variant_new_string("ltr");
        if (strcmp(property, "Status") == 0) return g_variant_new_string("normal");
        return g_variant_new_strv(NULL, 0);
    }
    if (strcmp(property, "Category") == 0) return g_variant_new_string("ApplicationStatus");
    if (strcmp(property, "Id") == 0) return g_variant_new_string("xpilot-infinity-tray");
    if (strcmp(property, "Title") == 0) return g_variant_new_string(sni->menu->items[0].label);
    if (strcmp(property, "Status") == 0) return g_variant_new_string("Active");
    if (strcmp(property, "WindowId") == 0) return g_variant_new_int32(0);
    if (strcmp(property, "Menu") == 0) return g_variant_new_object_path(MENU_PATH);
    if (strcmp(property, "ItemIsMenu") == 0) return g_variant_new_boolean(TRUE);
    if (strcmp(property, "IconPixmap") == 0) return g_variant_ref(sni->icons);
    if (g_str_has_suffix(property, "IconPixmap"))
        return g_variant_new_array(G_VARIANT_TYPE("(iiay)"), NULL, 0);
    if (strcmp(property, "ToolTip") == 0)
        return g_variant_new("(s@a(iiay)ss)", "", g_variant_ref(sni->icons),
                             "XPilot Infinity", sni->menu->items[0].label);
    return g_variant_new_string("");
}

static void registered(GObject *object, GAsyncResult *result, void *data)
{
    host_call *call = data;
    GError *error = NULL;
    GVariant *reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), result, &error);
    if (current(call))
        availability(call->sni, reply != NULL);
    g_clear_pointer(&reply, g_variant_unref);
    g_clear_error(&error);
    free_call(call);
}

static void host_ready(GObject *object, GAsyncResult *result, void *data)
{
    host_call *call = data;
    tray_sni *sni = call->sni;
    GError *error = NULL;
    GVariant *reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), result, &error);
    if (current(call)) {
        GVariant *value = NULL;
        if (reply)
            g_variant_get(reply, "(v)", &value);
        if (value && g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN)
            && g_variant_get_boolean(value)) {
            g_dbus_connection_call(sni->bus, sni->owner, WATCHER_PATH, WATCHER,
                "RegisterStatusNotifierItem", g_variant_new("(s)", ITEM_PATH),
                G_VARIANT_TYPE_UNIT, G_DBUS_CALL_FLAGS_NONE, -1, sni->cancel, registered, new_call(sni));
        } else {
            availability(sni, false);
        }
        g_clear_pointer(&value, g_variant_unref);
    }
    g_clear_pointer(&reply, g_variant_unref);
    g_clear_error(&error);
    free_call(call);
}

static void query_host(tray_sni *sni)
{
    sni->epoch++;
    g_dbus_connection_call(sni->bus, sni->owner, WATCHER_PATH,
        "org.freedesktop.DBus.Properties", "Get",
        g_variant_new("(ss)", WATCHER, "IsStatusNotifierHostRegistered"),
        G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NONE, -1, sni->cancel, host_ready, new_call(sni));
}

static void appeared(GDBusConnection *bus, const char *name, const char *owner, void *data)
{
    tray_sni *sni = data;
    (void)bus; (void)name;
    g_free(sni->owner);
    sni->owner = g_strdup(owner);
    query_host(sni);
}

static void vanished(GDBusConnection *bus, const char *name, void *data)
{
    tray_sni *sni = data;
    (void)bus; (void)name;
    sni->epoch++;
    g_clear_pointer(&sni->owner, g_free);
    availability(sni, false);
}

static void host_signal(GDBusConnection *bus, const char *sender, const char *path,
    const char *interface, const char *signal, GVariant *parameters, void *data)
{
    tray_sni *sni = data;
    (void)bus; (void)path; (void)parameters;
    if (!sni->alive || !sni->owner || strcmp(sender, sni->owner))
        return;
    if (strcmp(interface, "org.freedesktop.DBus.Properties") == 0
        || strcmp(signal, "StatusNotifierHostRegistered") == 0
        || strcmp(signal, "StatusNotifierHostUnregistered") == 0)
        query_host(sni);
}

tray_sni *tray_sni_create(GDBusConnection *bus, const tray_menu *menu,
    const char *icon_path, void (*activate)(void *, int),
    void (*available)(void *, bool), void *context, GError **error)
{
    tray_sni *sni = g_new0(tray_sni, 1);
    sni->references = 1;
    sni->alive = true;
    sni->bus = g_object_ref(bus);
    sni->cancel = g_cancellable_new();
    sni->menu = menu;
    sni->revision = menu->revision;
    sni->activate = activate;
    sni->available = available;
    sni->context = context;
    sni->icons = load_icons(icon_path, error);
    if (!sni->icons) {
        release(sni);
        return NULL;
    }
    GDBusNodeInfo *info = g_dbus_node_info_new_for_xml(interfaces, error);
    if (!info) {
        release(sni);
        return NULL;
    }
    const GDBusInterfaceVTable vtable = {method_called, get_property, NULL, {0}};
    sni->item_id = g_dbus_connection_register_object(bus, ITEM_PATH,
        info->interfaces[0], &vtable, sni, NULL, error);
    if (sni->item_id)
        sni->menu_id = g_dbus_connection_register_object(bus, MENU_PATH,
            info->interfaces[1], &vtable, sni, NULL, error);
    g_dbus_node_info_unref(info);
    if (!sni->item_id || !sni->menu_id) {
        tray_sni_destroy(sni);
        return NULL;
    }
    sni->signals = g_dbus_connection_signal_subscribe(bus, WATCHER, WATCHER,
        NULL, WATCHER_PATH, NULL, G_DBUS_SIGNAL_FLAGS_NONE, host_signal, retain(sni), release);
    sni->properties = g_dbus_connection_signal_subscribe(bus, WATCHER,
        "org.freedesktop.DBus.Properties", "PropertiesChanged", WATCHER_PATH, WATCHER,
        G_DBUS_SIGNAL_FLAGS_NONE, host_signal, retain(sni), release);
    sni->watch = g_bus_watch_name_on_connection(bus, WATCHER,
        G_BUS_NAME_WATCHER_FLAGS_NONE, appeared, vanished, retain(sni), release);
    return sni;
}

void tray_sni_update(tray_sni *sni)
{
    if (sni->revision == sni->menu->revision)
        return;
    sni->revision = sni->menu->revision;
    g_dbus_connection_emit_signal(sni->bus, NULL, MENU_PATH, MENU, "LayoutUpdated",
        g_variant_new("(ui)", sni->revision, 0), NULL);
    GVariantBuilder changed, removed;
    g_variant_builder_init(&changed, G_VARIANT_TYPE("a(ia{sv})"));
    g_variant_builder_init(&removed, G_VARIANT_TYPE("a(ias)"));
    for (size_t i = 0; i < G_N_ELEMENTS(sni->menu->items); i++)
        g_variant_builder_add(&changed, "(i@a{sv})", sni->menu->items[i].id,
            row_properties(&sni->menu->items[i]));
    g_dbus_connection_emit_signal(sni->bus, NULL, MENU_PATH, MENU, "ItemsPropertiesUpdated",
        g_variant_new("(a(ia{sv})a(ias))", &changed, &removed), NULL);
    g_dbus_connection_emit_signal(sni->bus, NULL, ITEM_PATH, ITEM, "NewTitle", NULL, NULL);
    g_dbus_connection_emit_signal(sni->bus, NULL, ITEM_PATH, ITEM, "NewToolTip", NULL, NULL);
}

void tray_sni_destroy(tray_sni *sni)
{
    if (!sni)
        return;
    sni->alive = false;
    g_cancellable_cancel(sni->cancel);
    if (sni->watch) g_bus_unwatch_name(sni->watch);
    if (sni->signals) g_dbus_connection_signal_unsubscribe(sni->bus, sni->signals);
    if (sni->properties) g_dbus_connection_signal_unsubscribe(sni->bus, sni->properties);
    if (sni->item_id) g_dbus_connection_unregister_object(sni->bus, sni->item_id);
    if (sni->menu_id) g_dbus_connection_unregister_object(sni->bus, sni->menu_id);
    release(sni);
}

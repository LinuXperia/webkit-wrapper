#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

static gchar *languages[2][8];
static gboolean permissions[4];

static gboolean permission_request (WebKitWebView *web_view,
                                    WebKitPermissionRequest *request,
                                    gpointer user_data)
{
    gboolean b;

    /* Look up if the user has granted permission for the specific
     * kind of request. */

    if (WEBKIT_IS_GEOLOCATION_PERMISSION_REQUEST(request)) {
        b = permissions[0];
    } else if (WEBKIT_IS_NOTIFICATION_PERMISSION_REQUEST(request)) {
        b = permissions[1];
    } else if (WEBKIT_IS_USER_MEDIA_PERMISSION_REQUEST(request)) {
        WebKitUserMediaPermissionRequest *media;

        media = WEBKIT_USER_MEDIA_PERMISSION_REQUEST(request);
        if (webkit_user_media_permission_is_for_audio_device(media)) {
            b = permissions[2];
        } else if (webkit_user_media_permission_is_for_video_device(media)) {
            b = permissions[3];
        }
    } else {
        b = FALSE;
    }

    /* Respond accordingly. */

    if (b) {
        webkit_permission_request_allow (request);
        g_debug("Allowed permission request.");
    } else {
        webkit_permission_request_deny (request);
        g_debug("Denied permission request.");
    }

    return TRUE;
}

static gboolean show_notification (WebKitWebView *view,
                                   WebKitNotification *notification,
                                   gpointer user_data)
{
    /* printf ("**** %s\n%s\n", */
    /*         webkit_notification_get_title (notification), */
    /*         webkit_notification_get_body (notification)); */

    return FALSE;
}

static gboolean decide_policy (WebKitWebView *view,
                               WebKitPolicyDecision *decision,
                               WebKitPolicyDecisionType decision_type,
                               gpointer user_data)
{
    WebKitNavigationPolicyDecision *navigation;
    WebKitNavigationAction *action;
    WebKitNavigationType type;
    WebKitURIRequest *request;

    switch (decision_type) {
    case WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION:
    case WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION:
        navigation = WEBKIT_NAVIGATION_POLICY_DECISION (decision);
        action = webkit_navigation_policy_decision_get_navigation_action (navigation);
        type = webkit_navigation_action_get_navigation_type(action);

        switch (type) {
        case WEBKIT_NAVIGATION_TYPE_LINK_CLICKED:
            request = webkit_navigation_action_get_request(action);

            {
                GError *error = NULL;

                if (!g_app_info_launch_default_for_uri (
                        webkit_uri_request_get_uri(request), NULL, &error) &&
                    error != NULL) {
                    fprintf (stderr, "Could not launch URI: %s\n",
                             error->message);
                    g_error_free (error);
                }
            }

            break;

        case WEBKIT_NAVIGATION_TYPE_RELOAD:
        case WEBKIT_NAVIGATION_TYPE_FORM_SUBMITTED:
        case WEBKIT_NAVIGATION_TYPE_FORM_RESUBMITTED:
        case WEBKIT_NAVIGATION_TYPE_OTHER:
            return FALSE;

        case WEBKIT_NAVIGATION_TYPE_BACK_FORWARD:
            webkit_policy_decision_ignore(decision);
            break;
        }

        break;
    default:
        /* Making no decision results in webkit_policy_decision_use(). */
        return FALSE;
    }

    return TRUE;
}

static void destroy_window(GtkWidget *widget, GtkWidget *window)
{
    gtk_main_quit();
}

static gboolean close_web_view(WebKitWebView *view, GtkWidget *window)
{
    gtk_widget_destroy(window);
    return TRUE;
}

static gboolean get_favicon(WebKitWebView *view,
                            GParamSpec *pspec,
                            GtkWindow *window)
{
    cairo_surface_t* surface;
    GdkPixbuf *pixbuf;

    if ((surface = webkit_web_view_get_favicon (view)) &&
        (pixbuf = gdk_pixbuf_get_from_surface (
            surface, 0, 0,
            cairo_image_surface_get_width (surface),
            cairo_image_surface_get_height (surface)))
        ) {
        gtk_window_set_icon (window, pixbuf);
        /* g_object_unref (pixbuf); */

        g_debug ("Set application icon from site favicon.\n");
    }

    return TRUE;
}

static gboolean get_title(WebKitWebView *view,
                          GParamSpec *pspec,
                          GtkWindow *window)
{
    gtk_window_set_title (window, webkit_web_view_get_title (view));
    return TRUE;
}

static gboolean context_menu_handler (WebKitWebView *view,
                                      WebKitContextMenu *menu,
                                      GdkEvent *event,
                                      WebKitHitTestResult *hit_test_result,
                                      gpointer user_data)
{
    if (webkit_hit_test_result_context_is_image(hit_test_result)) {
        GList *l, *n;

        for (l = webkit_context_menu_get_items(menu) ; l ; l = n) {
            WebKitContextMenuAction a;

            n = l->next;
            a = webkit_context_menu_item_get_stock_action(l->data);

            if (a != WEBKIT_CONTEXT_MENU_ACTION_COPY_IMAGE_TO_CLIPBOARD &&
                a != WEBKIT_CONTEXT_MENU_ACTION_COPY_IMAGE_URL_TO_CLIPBOARD) {
                webkit_context_menu_remove(menu, l->data);
            }
        }

        return FALSE;
    } else {
        return !(webkit_hit_test_result_context_is_editable(hit_test_result) ||
                 webkit_hit_test_result_context_is_selection(hit_test_result));
    }
}

static gboolean add_language (const gchar *option_name,
                              const gchar *value,
                              gpointer data,
                              GError **error)
{
    int i, j;

    if (!strcmp(option_name, "--lang") ||
        !strcmp(option_name, "-l")) {
        j = 0;
    } else if (!strcmp(option_name, "--spell") ||
        !strcmp(option_name, "-s")) {
        j = 1;
    } else {
        g_assert_not_reached();
    }

    for (i = 0 ; languages[j][i] && i < 8 ; i += 1);

    if (i == 7) {
        g_set_error(error,
                    G_OPTION_ERROR,
                    G_OPTION_ERROR_FAILED,
                    "Can't set more than 7 languages.");

        return FALSE;
    }

    languages[j][i] = g_strdup(value);

    return TRUE;
}

static gboolean add_permission (const gchar *option_name,
                                const gchar *value,
                                gpointer data,
                                GError **error)
{
    if (!strcmp(value, "geolocation")) {
        permissions[0] = TRUE;
    } else if (!strcmp(value, "notification")) {
        permissions[1] = TRUE;
    } else if (!strcmp(value, "audio")) {
        permissions[2] = TRUE;
    } else if (!strcmp(value, "video")) {
        permissions[3] = TRUE;
    } else {
        g_set_error(error,
                    G_OPTION_ERROR,
                    G_OPTION_ERROR_BAD_VALUE,
                    "Unknown type of permission request '%s' "
                    "supplied to --permit.", value);

        return FALSE;
    }

    return TRUE;
}

int main(int argc, char* argv[])
{
    char *url;

    GtkWidget *window;
    WebKitWebView *view;
    WebKitWebContext *context;

    GError *error = NULL;

    GOptionEntry options[] = {
        {"permit", 'p', G_OPTION_FLAG_NONE, G_OPTION_ARG_CALLBACK,
         &add_permission, "Permit a certain type of request.", "REQUEST"},
        {"spell", 's', G_OPTION_FLAG_NONE, G_OPTION_ARG_CALLBACK,
         &add_language, "Add spell checker language.", "LANG"},
        {"lang", 'l', G_OPTION_FLAG_NONE, G_OPTION_ARG_CALLBACK,
         &add_language, "Add preferred language.", "LANG"},
        {"url", 'u', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &url,
         "The wrapped URL.", "URL"},
        {NULL}
    };

    if (!gtk_init_with_args(&argc, &argv,
                            "- wrap a WebKit view around a URL",
                            options, NULL, &error)) {
        fprintf (stderr, "%s: %s\n", g_get_prgname(), error->message);
        g_error_free (error);

        return 1;
    }

    /* Create a WebKit context. */

    {
        WebKitWebsiteDataManager *manager;
        char *base, *home;

        int i, j;

        home = getenv("HOME");
        if (!home) {
            home = "/tmp";
        }

        base = strcat(home, "/.cache/webkit-wrapper");
        g_debug("Setting base directory to %s", base);

        manager = webkit_website_data_manager_new ("base-cache-directory", base,
                                                   "base-data-directory", base,
                                                   NULL);

        context = webkit_web_context_new_with_website_data_manager(manager);
        webkit_web_context_set_favicon_database_directory(context, base);

        /* Set langauge-related stuff. */

        if(languages[0][0]) {
            webkit_web_context_set_preferred_languages(
                context,
                (const gchar * const *)languages[0]);
        }

        if(languages[1][0]) {
            webkit_web_context_set_spell_checking_languages (
                context, (const gchar * const *)languages[1]);

            webkit_web_context_set_spell_checking_enabled (context, TRUE);
        }

        /* Free the langague vectors. */

        for (j = 0 ; j < 2 ; j += 1) {
            for (i = 0 ; languages[j][i] ; i += 1) {
                g_free(languages[j][i]);
            }
        }
    }

    /* Initialize the Gtk and create a window and view. */

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    view = WEBKIT_WEB_VIEW(webkit_web_view_new_with_context(context));
    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(view));

    /* Set up signals. */

    g_signal_connect(window, "destroy", G_CALLBACK(destroy_window), NULL);
    g_signal_connect(view, "close", G_CALLBACK(close_web_view), window);
    g_signal_connect(view, "permission-request",
                     G_CALLBACK(permission_request), NULL);
    g_signal_connect (view, "show-notification",
                      G_CALLBACK (show_notification), NULL);
    g_signal_connect (view, "decide-policy",
                      G_CALLBACK (decide_policy), NULL);
    g_signal_connect(view, "notify::favicon", G_CALLBACK(get_favicon), window);
    g_signal_connect(view, "notify::title", G_CALLBACK(get_title), window);
    g_signal_connect (view, "context-menu",
                      G_CALLBACK (context_menu_handler), NULL);

    /* Load the specified web page and show the window. */

    g_debug("Loading URL.");

    webkit_web_view_load_uri(view, url);

    gtk_widget_grab_focus(GTK_WIDGET(view));
    gtk_widget_show_all(window);

    /* Run the main GTK+ event loop. */

    gtk_main();

    return 0;
}

/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include "config.h"
#include <glib/gi18n-lib.h>
#include <gnome-keyring.h>

#include "goabackendprovider.h"
#include "goabackendgoogleprovider.h"
#include "goabackendfacebookprovider.h"
#include "goabackendyahooprovider.h"

/**
 * SECTION:goabackendprovider
 * @title: GoaBackendProvider
 * @short_description: Abstract base class for providers
 *
 * #GoaBackendProvider is the base type for all providers.
 */

static void goa_backend_provider_ensure_credentials_real (GoaBackendProvider   *provider,
                                                          GoaObject            *object,
                                                          GCancellable         *cancellable,
                                                          GAsyncReadyCallback   callback,
                                                          gpointer              user_data);

static gboolean goa_backend_provider_ensure_credentials_finish_real (GoaBackendProvider  *provider,
                                                                     gint                *out_expires_in,
                                                                     GAsyncResult        *res,
                                                                     GError             **error);

G_DEFINE_ABSTRACT_TYPE (GoaBackendProvider, goa_backend_provider, G_TYPE_OBJECT);

static void
goa_backend_provider_init (GoaBackendProvider *client)
{
}

static void
goa_backend_provider_class_init (GoaBackendProviderClass *klass)
{
  klass->ensure_credentials = goa_backend_provider_ensure_credentials_real;
  klass->ensure_credentials_finish = goa_backend_provider_ensure_credentials_finish_real;
}

/**
 * goa_backend_provider_get_provider_type:
 * @provider: A #GoaBackendProvider.
 *
 * Gets the type of @provider.
 *
 * This is a pure virtual method - a subclass must provide an
 * implementation.
 *
 * Returns: (transfer none): A string owned by @provider, do not free.
 */
const gchar *
goa_backend_provider_get_provider_type (GoaBackendProvider *provider)
{
  g_return_val_if_fail (GOA_IS_BACKEND_PROVIDER (provider), NULL);
  return GOA_BACKEND_PROVIDER_GET_CLASS (provider)->get_provider_type (provider);
}

/**
 * goa_backend_provider_get_name:
 * @provider: A #GoaBackendProvider.
 *
 * Gets a localized name for @provider that is suitable for display in
 * an user interface.
 *
 * This is a pure virtual method - a subclass must provide an
 * implementation.
 *
 * Returns: (transfer none): A string owned by @provider, do not free.
 */
const gchar *
goa_backend_provider_get_name (GoaBackendProvider *provider)
{
  g_return_val_if_fail (GOA_IS_BACKEND_PROVIDER (provider), NULL);
  return GOA_BACKEND_PROVIDER_GET_CLASS (provider)->get_name (provider);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * goa_backend_provider_add_account:
 * @provider: A #GoaBackendProvider.
 * @client: A #GoaClient.
 * @dialog: A #GtkDialog.
 * @vbox: A vertically oriented #GtkBox to put content in.
 * @error: Return location for error or %NULL.
 *
 * This method brings up the user interface necessary to create a new
 * account on @client of the type for @provider, interacts with the
 * user to get all information needed and creates the account.
 *
 * The passed in @dialog widget is guaranteed to be visible with @vbox
 * being empty and the only visible widget in @dialog's content
 * area. The dialog has exactly one action widget, a cancel button
 * with response id GTK_RESPONSE_CANCEL. Implementations are free to
 * add additional action widgets, as needed.
 *
 * If an account was successfully created, a #GoaObject for the
 * created account is returned. If @dialog is dismissed, %NULL is
 * returned and @error is set to %GOA_ERROR_DIALOG_DISMISSED. If an
 * account couldn't be created then @error is set.
 *
 * The caller will always show an error dialog if @error is set unless
 * the error is %GOA_ERROR_DIALOG_DISMISSED.
 *
 * Implementations should run the <link
 * linkend="g_main_context_default">default main loop</link> while
 * interacting with the user and may do so using e.g. gtk_dialog_run()
 * on @dialog.
 *
 * This is a pure virtual method - a subclass must provide an
 * implementation.
 *
 * Returns: The #GoaObject for the created account (must be relased
 *   with g_object_unref()) or %NULL if @error is set.
 */
GoaObject *
goa_backend_provider_add_account (GoaBackendProvider *provider,
                                  GoaClient          *client,
                                  GtkDialog          *dialog,
                                  GtkBox             *vbox,
                                  GError            **error)
{
  GoaObject *ret;

  g_return_val_if_fail (GOA_IS_BACKEND_PROVIDER (provider), NULL);
  g_return_val_if_fail (GOA_IS_CLIENT (client), NULL);
  g_return_val_if_fail (GTK_IS_DIALOG (dialog), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  ret = GOA_BACKEND_PROVIDER_GET_CLASS (provider)->add_account (provider, client, dialog, vbox, error);

  g_warn_if_fail ((ret == NULL && (error == NULL || *error != NULL)) || GOA_IS_OBJECT (ret));

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * goa_backend_provider_refresh_account:
 * @provider: A #GoaBackendProvider.
 * @client: A #GoaClient.
 * @object: A #GoaObject with a #GoaAccount interface.
 * @parent: (allow-none): Transient parent of dialogs or %NULL.
 * @error: Return location for error or %NULL.
 *
 * This method brings up the user interface necessary for refreshing
 * the credentials for the account specified by @object. This
 * typically involves having the user log in to the account again.
 *
 * Implementations should use @parent (unless %NULL) as the transient
 * parent of any created windows/dialogs.
 *
 * Implementations should run the <link
 * linkend="g_main_context_default">default main loop</link> while
 * interacting with the user.
 *
 * This is a pure virtual method - a subclass must provide an
 * implementation.
 *
 * Returns: %TRUE if the account has been refreshed, %FALSE if @error
 * is set.
 */
gboolean
goa_backend_provider_refresh_account (GoaBackendProvider  *provider,
                                      GoaClient           *client,
                                      GoaObject           *object,
                                      GtkWindow           *parent,
                                      GError             **error)
{
  g_return_val_if_fail (GOA_IS_BACKEND_PROVIDER (provider), FALSE);
  g_return_val_if_fail (GOA_IS_CLIENT (client), FALSE);
  g_return_val_if_fail (GOA_IS_OBJECT (object) && goa_object_peek_account (object) != NULL, FALSE);
  g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return GOA_BACKEND_PROVIDER_GET_CLASS (provider)->refresh_account (provider, client, object, parent, error);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * goa_backend_provider_build_object:
 * @provider: A #GoaBackendProvider.
 * @object: The #GoaObjectSkeleton that is being built.
 * @key_file: The #GKeyFile with configuation data.
 * @group: The group in @key_file to get data from.
 * @error: Return location for error or %NULL.
 *
 * This method is called when construction account #GoaObject<!-- -->
 * from configuration data - it basically provides a way to add
 * provider-specific information.
 *
 * The passed in @object will have a #GoaAccount interface
 * set. Implementations should validate and use data from @key_file to
 * add more interfaces to @object.
 *
 * Note that this may be called on already exported objects - for
 * example on configuration files reload.
 *
 * This is a pure virtual method - a subclass must provide an
 * implementation.
 *
 * Returns: %TRUE if data was valid, %FALSE if @error is set.
 */
gboolean
goa_backend_provider_build_object (GoaBackendProvider  *provider,
                                   GoaObjectSkeleton   *object,
                                   GKeyFile            *key_file,
                                   const gchar         *group,
                                   GError             **error)
{
  g_return_val_if_fail (GOA_IS_BACKEND_PROVIDER (provider), FALSE);
  g_return_val_if_fail (GOA_IS_OBJECT_SKELETON (object) && goa_object_peek_account (GOA_OBJECT (object)) != NULL, FALSE);
  g_return_val_if_fail (key_file != NULL, FALSE);
  g_return_val_if_fail (group != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  return GOA_BACKEND_PROVIDER_GET_CLASS (provider)->build_object (provider, object, key_file, group, error);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * goa_backend_provider_ensure_credentials:
 * @provider: A #GoaBackendProvider.
 * @object: A #GoaObject with a #GoaAccount interface.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @callback: The function to call when the request is satisfied.
 * @user_data: Pointer to pass to @callback.
 *
 * Ensures that credentials for @object are still valid.
 *
 * When the result is ready, @callback will be called in the the <link
 * linkend="g-main-context-push-thread-default">thread-default main
 * loop</link> this function was called from. You can then call
 * goa_backend_provider_ensure_credentials_finish() to get the result
 * of the operation.
 *
 * This is a virtual method where the default implemention simply returns
 * the %GOA_ERROR_NOT_SUPPORTED error. A subclass may provide
 * another implementation.
 */
void
goa_backend_provider_ensure_credentials (GoaBackendProvider   *provider,
                                         GoaObject            *object,
                                         GCancellable         *cancellable,
                                         GAsyncReadyCallback   callback,
                                         gpointer              user_data)
{
  g_return_if_fail (GOA_IS_BACKEND_PROVIDER (provider));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  GOA_BACKEND_PROVIDER_GET_CLASS (provider)->ensure_credentials (provider, object, cancellable, callback, user_data);
}

/**
 * goa_backend_provider_ensure_credentials_finish:
 * @provider: A #GoaBackendProvider.
 * @out_expires_in: (out): Return location for how long the expired credentials are good for (0 if unknown) or %NULL.
 * @res: A #GAsyncResult obtained from the #GAsyncReadyCallback passed to goa_backend_provider_ensure_credentials().
 * @error: Return location for error or %NULL.
 *
 * Finishes an operation started with goa_backend_provider_ensure_credentials().
 *
 * Returns: %TRUE if the credentials for the passed #GoaObject are valid, %FALSE if @error is set.
 */
gboolean
goa_backend_provider_ensure_credentials_finish (GoaBackendProvider  *provider,
                                                gint                *out_expires_in,
                                                GAsyncResult        *res,
                                                GError             **error)
{
  g_return_val_if_fail (GOA_IS_BACKEND_PROVIDER (provider), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  return GOA_BACKEND_PROVIDER_GET_CLASS (provider)->ensure_credentials_finish (provider, out_expires_in, res, error);
}

static void
goa_backend_provider_ensure_credentials_real (GoaBackendProvider   *provider,
                                              GoaObject            *object,
                                              GCancellable         *cancellable,
                                              GAsyncReadyCallback   callback,
                                              gpointer              user_data)
{
  GSimpleAsyncResult *simple;
  simple = g_simple_async_result_new (G_OBJECT (provider),
                                      callback,
                                      user_data,
                                      goa_backend_provider_ensure_credentials_real);
  g_simple_async_result_set_error (simple,
                                   GOA_ERROR,
                                   GOA_ERROR_NOT_SUPPORTED,
                                   "Not implemented");
  g_simple_async_result_complete_in_idle (simple);
  g_object_unref (simple);
}

static gboolean
goa_backend_provider_ensure_credentials_finish_real (GoaBackendProvider  *provider,
                                                     gint                *out_expires_in,
                                                     GAsyncResult        *res,
                                                     GError             **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  g_return_val_if_fail (GOA_IS_BACKEND_PROVIDER (provider), FALSE);
  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;
  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
ensure_ep_and_builtins (void)
{
  static gsize once_init_value = 0;

  if (g_once_init_enter (&once_init_value))
    {
      GIOExtensionPoint *extension_point;
      static volatile GType type = 0;

      extension_point = g_io_extension_point_register (GOA_BACKEND_PROVIDER_EXTENSION_POINT_NAME);
      g_io_extension_point_set_required_type (extension_point, GOA_TYPE_BACKEND_PROVIDER);

      type = GOA_TYPE_BACKEND_GOOGLE_PROVIDER;
      type = GOA_TYPE_BACKEND_FACEBOOK_PROVIDER;
      type = GOA_TYPE_BACKEND_YAHOO_PROVIDER;
      type = type; /* for -Wunused-but-set-variable */

      g_once_init_leave (&once_init_value, 1);
    }
}


/* ---------------------------------------------------------------------------------------------------- */

/**
 * goa_backend_provider_get_for_provider_type:
 * @provider_type: A provider type.
 *
 * Looks up the %GOA_BACKEND_PROVIDER_EXTENSION_POINT_NAME extension
 * point and returns a newly created #GoaBackendProvider for
 * @provider_type, if any.
 *
 * Returns: (transfer full): A #GoaBackendProvider (that must be freed
 * with g_object_unref()) or %NULL if not found.
 */
GoaBackendProvider *
goa_backend_provider_get_for_provider_type (const gchar *provider_type)
{
  GIOExtension *extension;
  GIOExtensionPoint *extension_point;
  GoaBackendProvider *ret;

  ensure_ep_and_builtins ();

  ret = NULL;

  extension_point = g_io_extension_point_lookup (GOA_BACKEND_PROVIDER_EXTENSION_POINT_NAME);
  extension = g_io_extension_point_get_extension_by_name (extension_point, provider_type);
  if (extension != NULL)
    ret = GOA_BACKEND_PROVIDER (g_object_new (g_io_extension_get_type (extension), NULL));
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * goa_backend_provider_get_all:
 *
 * Looks up the %GOA_BACKEND_PROVIDER_EXTENSION_POINT_NAME extension
 * point and returns a newly created #GoaBackendProvider for each
 * provider type encountered.
 *
 * Returns: (transfer full) (element-type GoaBackendProvider): A list
 *   of element providers that should be freed with g_list_free()
 *   after each element has been freed with g_object_unref().
 */
GList *
goa_backend_provider_get_all (void)
{
  GList *ret;
  GList *extensions;
  GList *l;
  GIOExtensionPoint *extension_point;

  ensure_ep_and_builtins ();

  ret = NULL;
  extension_point = g_io_extension_point_lookup (GOA_BACKEND_PROVIDER_EXTENSION_POINT_NAME);
  extensions = g_io_extension_point_get_extensions (extension_point);
  /* TODO: what if there are two extensions with the same name? */
  for (l = extensions; l != NULL; l = l->next)
    {
      GIOExtension *extension = l->data;
      ret = g_list_prepend (ret, g_object_new (g_io_extension_get_type (extension), NULL));
    }
  ret = g_list_reverse (ret);
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static const GnomeKeyringPasswordSchema keyring_password_schema =
{
  GNOME_KEYRING_ITEM_GENERIC_SECRET,
  {
    { "goa-identity", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { NULL, 0 }
  }
};

static void
store_password_cb (GnomeKeyringResult result,
                   gpointer           user_data)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);

  if (result != GNOME_KEYRING_RESULT_OK)
    {
      g_simple_async_result_set_error (simple,
                                       GOA_ERROR,
                                       GOA_ERROR_FAILED,
                                       _("Failed to store credentials in the keyring: %s"),
                                       gnome_keyring_result_to_message (result));
    }
  g_simple_async_result_complete_in_idle (simple);
  g_object_unref (simple);
}

/**
 * goa_backend_provider_store_credentials:
 * @provider: A #GoaBackendProvider.
 * @identity: The identity to store credentials for.
 * @credentials: The credentials to store.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @callback: The function to call when the request is satisfied.
 * @user_data: Pointer to pass to @callback.
 *
 * Stores @credentials for @identity in the key-ring. If @credentials
 * is floating, it is consumed.
 *
 * When the result is ready, @callback will be called in the the <link
 * linkend="g-main-context-push-thread-default">thread-default main
 * loop</link> this function was called from. You can then call
 * goa_backend_provider_store_credentials_finish() to get the result
 * of the operation.
 *
 * This is a convenience method (not virtual) that subclasses can use.
 */
void
goa_backend_provider_store_credentials (GoaBackendProvider   *provider,
                                        const gchar          *identity,
                                        GVariant             *credentials,
                                        GCancellable         *cancellable,
                                        GAsyncReadyCallback   callback,
                                        gpointer              user_data)
{
  GSimpleAsyncResult *simple;
  gchar *credentials_str;
  gchar *password_description;
  gchar *password_key;

  g_return_if_fail (GOA_IS_BACKEND_PROVIDER (provider));
  g_return_if_fail (identity != NULL);
  g_return_if_fail (credentials != NULL);
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  /* TODO: use GCancellable */

  simple = g_simple_async_result_new (G_OBJECT (provider),
                                      callback,
                                      user_data,
                                      goa_backend_provider_store_credentials);

  credentials_str = g_variant_print (credentials, TRUE);
  g_variant_ref_sink (credentials);
  g_variant_unref (credentials);

  password_key = g_strdup_printf ("%s:%s",
                                  goa_backend_provider_get_provider_type (GOA_BACKEND_PROVIDER (provider)),
                                  identity);
  password_description = g_strdup_printf (_("GOA %s credentials for identity %s"),
                                          goa_backend_provider_get_provider_type (GOA_BACKEND_PROVIDER (provider)),
                                          identity);
  gnome_keyring_store_password (&keyring_password_schema,
                                NULL, /* default keyring */
                                password_description,
                                credentials_str,
                                store_password_cb,
                                simple,
                                NULL, /* GDestroyNotify */
                                "goa-identity", password_key,
                                NULL);

  g_free (credentials_str);
  g_free (password_key);
  g_free (password_description);
}

/**
 * goa_backend_provider_store_credentials_finish:
 * @provider: A #GoaBackendProvider.
 * @res: A #GAsyncResult obtained from the #GAsyncReadyCallback passed to goa_backend_provider_store_credentials().
 * @error: Return location for error or %NULL.
 *
 * Finishes an operation started with goa_backend_provider_store_credentials().
 *
 * This is a convenience method (not virtual) that subclasses can use.
 *
 * Returns: %TRUE if the credentials was successfully stored, %FALSE
 * if @error is set.
 */
gboolean
goa_backend_provider_store_credentials_finish (GoaBackendProvider   *provider,
                                               GAsyncResult         *res,
                                               GError              **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  gboolean ret;

  g_return_val_if_fail (GOA_IS_BACKEND_PROVIDER (provider), FALSE);
  g_return_val_if_fail (g_simple_async_result_is_valid (res, G_OBJECT (provider), goa_backend_provider_store_credentials), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = FALSE;

  if (g_simple_async_result_propagate_error (simple, error))
    goto out;

  ret = TRUE;

 out:
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
find_password_cb (GnomeKeyringResult  result,
                  const gchar        *returned_password,
                  gpointer            user_data)
{
  GSimpleAsyncResult *simple = user_data;
  GVariant *ret;
  GError *error;

  if (result != GNOME_KEYRING_RESULT_OK)
    {
      g_simple_async_result_set_error (simple,
                                       GOA_ERROR,
                                       GOA_ERROR_FAILED, /* TODO: more specific */
                                       _("Failed to retrieve credentials from the keyring: %s"),
                                       gnome_keyring_result_to_message (result));
      goto out;
    }

  error = NULL;
  ret = g_variant_parse (NULL, /* GVariantType */
                         returned_password,
                         NULL, /* limit */
                         NULL, /* endptr */
                         &error);
  if (ret == NULL)
    {
      g_simple_async_result_take_error (simple, error);
      goto out;
    }
  if (g_variant_is_floating (ret))
    g_variant_ref_sink (ret);
  g_simple_async_result_set_op_res_gpointer (simple, ret, (GDestroyNotify) g_variant_unref);

 out:
  g_simple_async_result_complete_in_idle (simple);
  g_object_unref (simple);
}


/**
 * goa_backend_provider_lookup_credentials:
 * @provider: A #GoaBackendProvider.
 * @identity: The identity to look up credentials for.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @callback: The function to call when the request is satisfied.
 * @user_data: Pointer to pass to @callback.
 *
 * Looks up credentials in the keyring for @identity previously stored
 * with goa_backend_provider_store_credentials().
 *
 * When the result is ready, @callback will be called in the the <link
 * linkend="g-main-context-push-thread-default">thread-default main
 * loop</link> this function was called from. You can then call
 * goa_backend_provider_lookup_credentials_finish() to get the result
 * of the operation.
 *
 * This is a convenience method (not virtual) that subclasses can use.
 */
void
goa_backend_provider_lookup_credentials (GoaBackendProvider   *provider,
                                         const gchar          *identity,
                                         GCancellable         *cancellable,
                                         GAsyncReadyCallback   callback,
                                         gpointer              user_data)
{
  GSimpleAsyncResult *simple;
  gchar *password_key;

  g_return_if_fail (GOA_IS_BACKEND_PROVIDER (provider));
  g_return_if_fail (identity != NULL);
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  simple = g_simple_async_result_new (G_OBJECT (provider),
                                      callback,
                                      user_data,
                                      goa_backend_provider_lookup_credentials);

  password_key = g_strdup_printf ("%s:%s",
                                  goa_backend_provider_get_provider_type (GOA_BACKEND_PROVIDER (provider)),
                                  identity);

  gnome_keyring_find_password (&keyring_password_schema,
                               find_password_cb,
                               simple,
                               NULL, /* GDestroyNotify */
                               "goa-identity", password_key,
                               NULL);
  g_free (password_key);
}

/**
 * goa_backend_provider_lookup_credentials_finish:
 * @provider: A #GoaBackendProvider.
 * @res: A #GAsyncResult obtained from the #GAsyncReadyCallback passed to goa_backend_provider_lookup_credentials().
 * @error: Return location for error or %NULL.
 *
 * Finishes an operation started with goa_backend_provider_lookup_credentials().
 *
 * This is a convenience method (not virtual) that subclasses can use.
 *
 * Returns: (transfer full): A #GVariant (never floating)
 * with credentials or %NULL if @error is set. Free with
 * g_hash_table_unref().
 */
GVariant *
goa_backend_provider_lookup_credentials_finish (GoaBackendProvider   *provider,
                                                GAsyncResult         *res,
                                                GError              **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  GVariant *ret;

  g_return_val_if_fail (GOA_IS_BACKEND_PROVIDER (provider), FALSE);
  g_return_val_if_fail (g_simple_async_result_is_valid (res, G_OBJECT (provider), goa_backend_provider_lookup_credentials), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = NULL;

  if (g_simple_async_result_propagate_error (simple, error))
    goto out;

  ret = g_simple_async_result_get_op_res_gpointer (simple);
  g_assert (!g_variant_is_floating (ret));
  g_variant_ref (ret);

 out:
  return ret;
}


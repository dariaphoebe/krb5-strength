/*
 * The public APIs of the password strength checking kadmind plugin.
 *
 * Provides the public pwcheck_init, pwcheck_check, and pwcheck_close APIs for
 * the kadmind plugin.
 *
 * Developed by Derrick Brashear and Ken Hornstein of Sine Nomine Associates,
 *     on behalf of Stanford University.
 * Extensive modifications by Russ Allbery <rra@stanford.edu>
 * Copyright 2006, 2007, 2009, 2012, 2013
 *     The Board of Trustees of the Leland Stanford Junior Unversity
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/kadmin.h>
#include <portable/krb5.h>
#include <portable/system.h>

#ifdef HAVE_CDB_H
# include <cdb.h>
#endif
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <plugin/api.h>
#include <util/macros.h>

/* Heimdal doesn't define KADM5_PASS_Q_GENERIC. */
#ifndef KADM5_PASS_Q_GENERIC
# define KADM5_PASS_Q_GENERIC KADM5_PASS_Q_DICT
#endif

/* If not built with CDB support, provide some stubs. */
#ifndef HAVE_CDB
# define pwcheck_check_cdb(c, d, p) 0
# define pwcheck_close_cdb(c, d)    /* empty */
#endif

/* The public function exported by the cracklib library. */
extern char *FascistCheck(const char *password, const char *dict);


/*
 * Load a number option from Kerberos appdefaults.  Takes the PAM argument
 * struct, the section name, the realm, the option, and the result location.
 * The native interface doesn't support numbers, so we actually read a string
 * and then convert.
 */
static void
default_number(krb5_context ctx, const char *section, const char *opt,
               long *result)
{
    char *tmp = NULL;
    char *realm = NULL;
    char *end;
    long value;
    krb5_error_code code;
#ifdef HAVE_KRB5_REALM
    krb5_const_realm rdata = realm;
#else
    krb5_data realm_struct;
    const krb5_data *rdata;
#endif

    /* Get the default realm.  This is annoying for MIT Kerberos. */
    code = krb5_get_default_realm(ctx, &realm);
    if (code != 0)
        realm = NULL;
#ifdef HAVE_KRB5_REALM
    rdata = realm;
#else
    if (realm == NULL)
        rdata = NULL;
    else {
        rdata = &realm_struct;
        realm_struct.magic = KV5M_DATA;
        realm_struct.data = (void *) realm;
        realm_struct.length = strlen(realm);
    }
#endif

    /* Obtain the string from [appdefaults]. */
    krb5_appdefault_string(ctx, section, rdata, opt, "", &tmp);

    /*
     * If we found anything, convert it to a number.  Currently, we ignore
     * errors here.
     */
    if (tmp != NULL && tmp[0] != '\0') {
        errno = 0;
        value = strtol(tmp, &end, 10);
        if (errno == 0 && *end == '\0')
            *result = value;
    }
    if (tmp != NULL)
        free(tmp);
}


/*
 * Load a string option from Kerberos appdefaults.  Takes the Kerberos
 * context, the section name, the realm, the option, and the result location.
 *
 * This requires an annoying workaround because one cannot specify a default
 * value of NULL with MIT Kerberos, since MIT Kerberos unconditionally calls
 * strdup on the default value.  There's also no way to determine if memory
 * allocation failed while parsing or while setting the default value, so we
 * don't return an error code.
 */
static void
default_string(krb5_context ctx, const char *section, const char *opt,
               char **result)
{
    char *value = NULL;
    char *realm = NULL;
    krb5_error_code code;
#ifdef HAVE_KRB5_REALM
    krb5_const_realm rdata;
#else
    krb5_data realm_struct;
    const krb5_data *rdata;
#endif

    /* Get the default realm.  This is annoying for MIT Kerberos. */
    code = krb5_get_default_realm(ctx, &realm);
    if (code != 0)
        realm = NULL;
#ifdef HAVE_KRB5_REALM
    rdata = realm;
#else
    if (realm == NULL)
        rdata = NULL;
    else {
        rdata = &realm_struct;
        realm_struct.magic = KV5M_DATA;
        realm_struct.data = (void *) realm;
        realm_struct.length = strlen(realm);
    }
#endif

    /* Obtain the string from [appdefaults]. */
    krb5_appdefault_string(ctx, section, rdata, opt, "", &value);

    /* If we got something back, store it in result. */
    if (value != NULL) {
        if (value[0] == '\0')
            free(value);
        else {
            if (*result != NULL)
                free(*result);
            *result = strdup(value);
            krb5_free_string(ctx, value);
        }
    }

    /* Free the realm if we got one. */
    if (realm != NULL)
        krb5_free_default_realm(ctx, realm);
}


/*
 * Initialize the CrackLib dictionary.  Ensure that the dictionary file exists
 * and is readable and store the path in the module context.  Returns 0 on
 * success, non-zero on failure.
 *
 * The dictionary file should not include the trailing .pwd extension.
 * Currently, we don't cope with a NULL dictionary path.
 */
static krb5_error_code
init_cracklib(krb5_context ctx, krb5_pwqual_moddata data)
{
    char *file;
    int oerrno;

    /* Sanity-check the dictionary path. */
    if (asprintf(&file, "%s.pwd", data->dictionary) < 0) {
        oerrno = errno;
        krb5_set_error_message(ctx, oerrno, "cannot allocate memory: %s",
                               strerror(oerrno));
        return oerrno;
    }
    if (access(file, R_OK) != 0) {
        oerrno = errno;
        krb5_set_error_message(ctx, oerrno, "dictionary %s does not exist: %s",
                               file, strerror(oerrno));
        free(file);
        return oerrno;
    }
    free(file);
    return 0;
}


#ifdef HAVE_CDB
/*
 * Initialize the CDB dictionary.  Opens the dictionary and sets up the
 * TinyCDB state.  Returns 0 on success, non-zero on failure (and sets the
 * error in the Kerberos context).  If not built with CDB support, always
 * returns an error.
 */
static krb5_error_code
init_cdb(krb5_context ctx, krb5_pwqual_moddata data, const char *database)
{
    krb5_error_code code;

    data->cdb_fd = open(database, O_RDONLY);
    if (data->cdb_fd < 0) {
        code = errno;
        krb5_set_error_message(ctx, code, "cannot open dictionary %s: %s",
                               database, strerror(errno));
        return code;
    }
    if (cdb_init(&data->cdb, data->cdb_fd) < 0) {
        code = errno;
        krb5_set_error_message(ctx, code,
                               "cannot initialize dictionary %s: %s",
                               database, strerror(errno));
        close(data->cdb_fd);
        data->cdb_fd = -1;
        return code;
    }
    data->have_cdb = true;
    return 0;
}

#else

/*
 * Stub for init_cdb if not built with CDB support.
 */
static krb5_error_code
init_cdb(krb5_context ctx, krb5_pwqual_moddata data UNUSED,
         const char *database UNUSED)
{
    krb5_error_code code;

    code = KADM5_BAD_SERVER_PARAMS;
    krb5_set_error_message(ctx, code,
        "CDB dictionary requested but not built with CDB support");
    return code;
}

#endif


/*
 * Initialize the module.  Ensure that the dictionary file exists and is
 * readable and store the path in the module context.  Returns 0 on success,
 * non-zero on failure.  This function returns failure only if it could not
 * allocate memory or internal Kerberos calls that shouldn't fail do.
 *
 * The dictionary file should not include the trailing .pwd extension.
 * Currently, we don't cope with a NULL dictionary path.
 */
krb5_error_code
pwcheck_init(krb5_context ctx, const char *dictionary,
             krb5_pwqual_moddata *moddata)
{
    krb5_pwqual_moddata data = NULL;
    char *cdb_path = NULL;
    krb5_error_code code;

    /* Allocate our internal data. */
    data = calloc(1, sizeof(*data));
    if (data == NULL) {
        code = errno;
        krb5_set_error_message(ctx, code, "cannot allocate memory: %s",
                               strerror(code));
        return code;
    }
    data->cdb_fd = -1;

    /* Get minimum length information from krb5.conf. */
    default_number(ctx, "krb5-strength", "minimum_length", &data->min_length);

    /* Use dictionary if given, otherwise get from krb5.conf. */
    if (dictionary == NULL)
        default_string(ctx, "krb5-strength", "password_dictionary",
                       &data->dictionary);
    else {
        data->dictionary = strdup(dictionary);
        if (data->dictionary == NULL) {
            code = errno;
            krb5_set_error_message(ctx, code, "cannot allocate memory");
            goto fail;
        }
    }

    /* Get CDB dictionary path from krb5.conf. */
    default_string(ctx, "krb5-strength", "password_dictionary_cdb", &cdb_path);

    /* If there is no dictionary, abort our setup with an error. */
    if (data->dictionary == NULL && cdb_path == NULL) {
        code = KADM5_MISSING_CONF_PARAMS;
        krb5_set_error_message(ctx, code,
            "password_dictionary not configured in krb5.conf");
        goto fail;
    }

    /* If there is a CrackLib dictionary, initialize CrackLib. */
    if (data->dictionary != NULL) {
        code = init_cracklib(ctx, data);
        if (code != 0)
            goto fail;
    }

    /* If there is a CDB dictionary, initialize TinyCDB. */
    if (cdb_path != NULL) {
        code = init_cdb(ctx, data, cdb_path);
        if (code != 0)
            goto fail;
    }

    /* Initialized.  Set moddata and return. */
    *moddata = data;
    return 0;

fail:
    if (data != NULL)
        pwcheck_close(ctx, data);
    free(cdb_path);
    *moddata = NULL;
    return code;
}


/*
 * Check a given password.  Takes a Kerberos context, our module data, the
 * password, the principal the password is for, and a buffer and buffer length
 * into which to put any failure message.
 */
krb5_error_code
pwcheck_check(krb5_context ctx UNUSED, krb5_pwqual_moddata data,
              const char *password, const char *principal)
{
    char *user, *p;
    const char *q;
    size_t i, j;
    char c;
    int oerrno;
    const char *result;
    krb5_error_code code;

    /* Check minimum length first, since that's easy. */
    if ((long) strlen(password) < data->min_length) {
        code = KADM5_PASS_Q_TOOSHORT;
        krb5_set_error_message(ctx, code, "password is too short");
        return code;
    }

    /*
     * We get the principal (in krb5_unparse_name format) from kadmind and we
     * want to be sure that the password doesn't match the username, the
     * username reversed, or the username with trailing digits.  We therefore
     * have to copy the string so that we can manipulate it a bit.
     */
    if (strcasecmp(password, principal) == 0) {
        code = KADM5_PASS_Q_GENERIC;
        krb5_set_error_message(ctx, code, "password based on username");
        return code;
    }
    user = strdup(principal);
    if (user == NULL) {
        oerrno = errno;
        krb5_set_error_message(ctx, oerrno, "cannot allocate memory: %s",
                               strerror(oerrno));
        return oerrno;
    }
    for (p = user; p[0] != '\0'; p++) {
        if (p[0] == '\\' && p[1] != '\0') {
            p++;
            continue;
        }
        if (p[0] == '@') {
            p[0] = '\0';
            break;
        }
    }
    if (strlen(password) == strlen(user)) {
        if (strcasecmp(password, user) == 0) {
            free(user);
            krb5_set_error_message(ctx, KADM5_PASS_Q_GENERIC,
                                   "password based on username");
            return KADM5_PASS_Q_GENERIC;
        }

        /* Check against the reversed username. */
        for (i = 0, j = strlen(user) - 1; i < j; i++, j--) {
            c = user[i];
            user[i] = user[j];
            user[j] = c;
        }
        if (strcasecmp(password, user) == 0) {
            free(user);
            krb5_set_error_message(ctx, KADM5_PASS_Q_GENERIC,
                                   "password based on username");
            return KADM5_PASS_Q_GENERIC;
        }
    }
    if (strlen(password) > strlen(user))
        if (strncasecmp(password, user, strlen(user)) == 0) {
            q = password + strlen(user);
            while (isdigit((int) *q))
                q++;
            if (*q == '\0') {
                free(user);
                krb5_set_error_message(ctx, KADM5_PASS_Q_GENERIC,
                                       "password based on username");
                return KADM5_PASS_Q_GENERIC;
            }
        }
    free(user);

    /* Check the password against CrackLib if it is configured. */
    if (data->dictionary != NULL) {
        result = FascistCheck(password, data->dictionary);
        if (result != NULL) {
            krb5_set_error_message(ctx, KADM5_PASS_Q_GENERIC, "%s", result);
            return KADM5_PASS_Q_GENERIC;
        }
    }

    /* Check the password against CDB if it is configured. */
    if (data->have_cdb) {
        code = pwcheck_check_cdb(ctx, data, password);
        if (code != 0)
            return code;
    }
    return 0;
}


/*
 * Cleanly shut down the password strength plugin.  The only thing we have to
 * do is free the memory allocated for our internal data.
 */
void
pwcheck_close(krb5_context ctx UNUSED, krb5_pwqual_moddata data)
{
    if (data != NULL) {
        pwcheck_close_cdb(ctx, data);
        free(data->dictionary);
        free(data);
    }
}

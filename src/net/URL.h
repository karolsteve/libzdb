/*
 * Copyright (C) Tildeslash Ltd. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 *
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.
 */


#ifndef URL_INCLUDED
#define URL_INCLUDED


/**
 * **URL** represents an immutable Uniform Resource Locator.
 * A Uniform Resource Locator (URL) is used to uniquely identify a
 * resource on the Internet. The URL is a compact text string with a
 * restricted syntax that consists of four main components:
 *
 * ```
 * <protocol>://<authority><path>?<query>
 * ```
 *
 * The `<protocol>` part is mandatory, the other components may or
 * may not be present in a URL string. For instance, the
 * `file` protocol only uses the path component while an
 * `http` protocol may use all components. Here is an
 * example where all components are used:
 *
 * ```
 * http://user:password@www.foo.bar:8080/document/index.csp?querystring#ref
 * ```
 *
 * The following URL components are automatically unescaped according to the escaping
 * mechanism defined in RFC 2396: `credentials`, `path`, and parameter
 * `values`.
 *
 * An *IPv6 address* can be used for host as defined in
 * [RFC2732](https://www.ietf.org/rfc/rfc2732.txt) by enclosing the
 * address in [brackets]. For instance, mysql://[2010:836B:4179::836B:4179]:3306/test
 *
 * For more information about the URL syntax and specification, see
 * [RFC2396 - Uniform Resource Identifiers (URI): Generic Syntax](https://www.ietf.org/rfc/rfc2396.txt)
 *
 * @file
 */


#define T URL_T
typedef struct URL_S *T;


/**
 * @brief Create a new URL object from the `url` parameter string.
 * @param url A string specifying the URL
 * @return A URL object or NULL if the `url` parameter
 * cannot be parsed as a URL.
 */
T URL_new(const char *url);


/**
 * @brief Build a new URL object from the `url` parameter string.
 *
 * Factory method for building a URL object using a variable argument
 * list. *Important*: since the '%%' character is used as a format
 * specifier (e.g. %%s for string, %%d for integer and so on),
 * submitting a URL escaped string (i.e. a %%HEXHEX encoded string)
 * in the `url` parameter can produce undesired results. In
 * this case, use either the URL_new() method or URL_unescape() the
 * `url` parameter first.
 *
 * @param url A string specifying the URL
 * @return A URL object or NULL if the `url` parameter
 * cannot be parsed as a URL.
 */
T URL_create(const char *url, ...) __attribute__((format (printf, 1, 2)));


/**
 * @brief Destroy a URL object.
 * @param U A URL object reference
 */
void URL_free(T *U);

/// @name Properties
/// @{

/**
 * @brief Gets the protocol of the URL.
 * @param U A URL object
 * @return The protocol name
 */
const char *URL_getProtocol(T U);


/**
 * @brief Gets the username from the URL's authority part.
 * @param U A URL object
 * @return A username specified in the URL or NULL if not found
 */
const char *URL_getUser(T U);


/**
 * @brief Gets the password from the URL's authority part.
 * @param U A URL object
 * @return A password specified in the URL or NULL if not found
 */
const char *URL_getPassword(T U);


/**
 * @brief Gets the hostname of the URL.
 * @param U A URL object
 * @return The hostname of the URL or NULL if not found
 */
const char *URL_getHost(T U);


/**
 * @brief Gets the port of the URL.
 * @param U A URL object
 * @return The port number of the URL or -1 if not specified
 */
int URL_getPort(T U);


/**
 * @brief Gets the path of the URL.
 * @param U A URL object
 * @return The path of the URL or NULL if not found
 */
const char *URL_getPath(T U);


/**
 * @brief Gets the query string of the URL.
 * @param U A URL object
 * @return The query string of the URL or NULL if not found
 */
const char *URL_getQueryString(T U);


/**
 * Returns an array of string objects with the names of the
 * parameters contained in this URL. If the URL has no parameters,
 * the method returns NULL. The last value in the array is NULL.
 * To print all parameter names and their values contained in this
 * URL, the following code can be used:
 * ```c
 * const char **params = URL_getParameterNames(U);
 * if (params) {
 *     for (int i = 0; params[i]; i++)
 *         printf("%s = %s\n", params[i], URL_getParameter(U, params[i]));
 * }
 * ```
 * @param U A URL object
 * @return An array of string objects, each string containing the name
 * of a URL parameter; or NULL if the URL has no parameters
 */
const char **URL_getParameterNames(T U);


/**
 * Returns the value of a URL parameter as a string, or NULL if
 * the parameter does not exist. If you use this method with a
 * multi-valued parameter, the value returned is the first value found.
 * Lookup is *case-sensitive*.
 * @param U A URL object
 * @param name The parameter name to lookup
 * @return The parameter value or NULL if not found
 */
const char *URL_getParameter(T U, const char *name);

/// @}
/// @name Functions
/// @{

/**
 * @brief Returns a string representation of this URL object.
 * @param U A URL object
 * @return The URL string
 */
const char *URL_toString(T U);

/// @}
/// @name Class functions
/// @{

/**
 * @brief Unescape a URL string.
 *
 * The `url` parameter is modified by this method.
 *
 * @param url an escaped URL string
 * @return A pointer to the unescaped `url` string
 */
char *URL_unescape(char *url);


/**
 * @brief Escape a URL string
 *
 * Converts unsafe characters to a hex (%HEXHEX) representation. The 
 * following URL unsafe characters are encoded: <code><>\"#%{}|^ []\`</code>
 * as well as characters in the interval 00-1F hex (0-31 decimal) and in
 * the interval 7F-FF (127-255 decimal). If the `url` parameter is NULL
 * then this method returns NULL, if it is the empty string "" a *new*
 * empty string is returned. _The caller must free the returned string._
 *
 * @param url a URL string
 * @return The escaped string.
 */
char *URL_escape(const char *url);

/// @}

#undef T
#endif

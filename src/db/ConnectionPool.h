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


#ifndef CONNECTIONPOOL_INCLUDED
#define CONNECTIONPOOL_INCLUDED


/**
 * A **ConnectionPool** represents a database connection pool.
 *
 * A connection pool can be used to get a connection to a database and
 * execute statements. This class opens a number of database
 * connections and allows callers to obtain and use a database connection in
 * a reentrant manner. Applications can instantiate as many ConnectionPool
 * objects as needed and against as many different database systems as needed.
 * The following diagram gives an overview of the library's components and
 * their method-associations:
 *
 * <center><img src="database.png"></center>
 *
 * The method ConnectionPool_getConnection() is used to obtain a new
 * connection from the pool. If there are no connections available, a new
 * connection is created and returned. If the pool has already handed out
 * *maxConnections* Connections, the next call to
 * ConnectionPool_getConnection() will return NULL. Use Connection_close()
 * to return a connection to the pool so it can be reused.
 *
 * A connection pool is created by default with 5 initial connections and
 * with 20 maximum connections. These values can be changed by the property
 * methods ConnectionPool_setInitialConnections() and
 * ConnectionPool_setMaxConnections().
 *
 * ## Supported database systems:
 * This library may be built with support for many different database
 * systems. To test if a particular system is supported, use the method
 * Connection_isSupported().
 *
 * ## Life-cycle methods:
 * Clients should call ConnectionPool_start() to establish the connection pool
 * against the database server before using the pool. To shutdown
 * connections from the database server, use ConnectionPool_stop(). Set
 * preferred properties *before* calling ConnectionPool_start(). Some
 * properties can also be changed dynamically after the pool was started, such as
 * changing the maximum number of connections or the number of initial connections.
 * Changing and tuning these properties at runtime is most useful if the pool was
 * started with a reaper-thread (see below) since the reaper dynamically changes the
 * size of the pool.
 *
 * ## Connection URL:
 * The URL given to a Connection Pool at creation time specifies a database
 * connection in the standard URL format. The format of the connection URL
 * is defined as:
 *
 * <pre>
 * database://[user:password@][host][:port]/database[?propertyName1][=propertyValue1][&propertyName2][=propertyValue2]...
 * </pre>
 *
 * The property names `user` and `password` are always
 * recognized and specify how to log in to the database. Other properties
 * depend on the database server in question. Username and password can
 * alternatively be specified in the auth-part of the URL. If port number is
 * omitted, the default port number for the database server is used.
 *
 * #### MySQL:
 *
 * Here is an example of how to connect to a [MySQL](http://www.mysql.org/)
 * database server:
 *
 * ```
 * mysql://localhost:3306/test?user=root&password=swordfish
 * ```
 *
 * In this case, the username, `root` and password, `swordfish`
 * are specified as properties to the URL. An alternative is to
 * use the auth-part of the URL to specify authentication information:
 *
 * ```
 * mysql://root:swordfish@localhost:3306/test
 * ```
 *
 * See [mysql options](mysqloptions.html) for all properties that
 * can be set for a mysql connection URL.
 *
 * #### SQLite:
 *
 * For a [SQLite](http://www.sqlite.org/) database, the connection
 * URL should simply specify a database file, since a SQLite database
 * is just a file in the filesystem. SQLite uses
 * [pragma commands](http://sqlite.org/pragma.html) for
 * performance tuning and other special purpose database commands. Pragma
 * syntax in the form `name=value` can be added as properties
 * to the URL and will be set when the Connection is created. In addition
 * to pragmas, the following properties are supported:
 *
 * - `heap_limit=value` - Make SQLite auto-release unused memory
 *   if memory usage goes above the specified value [KB].
 *
 * A URL for connecting to a SQLite database might look like this (with recommended pragmas):
 *
 * ```
 * sqlite:///var/sqlite/test.db?synchronous=normal&foreign_keys=on&journal_mode=wal&temp_store=memory
 * ```
 *
 * #### PostgreSQL:
 *
 * The URL for connecting to a [PostgreSQL](http://www.postgresql.org/)
 * database server might look like:
 *
 * ```
 * postgresql://localhost:5432/test?user=root&password=swordfish
 * ```
 *
 * As with the MySQL URL, the username and password are specified as
 * properties to the URL. Likewise, the auth-part of the URL can be used
 * instead to specify the username and the password:
 *
 * ```
 * postgresql://root:swordfish@localhost/test?use-ssl=true
 * ```
 *
 * In this example, we have also omitted the port number to the server, in
 * which case the default port number, *5432*, for PostgreSQL is used. In
 * addition, we have added an extra parameter to the URL, so connection to
 * the server is done over a secure SSL connection.
 *
 * See [postgresql options](postgresoptions.html) for all properties that
 * can be set for a postgresql connection URL.
 *
 * #### Oracle:
 *
 * The URL for connecting to an [Oracle](http://www.oracle.com/)
 * database server might look like:
 *
 * ```
 * oracle://localhost:1521/servicename?user=scott&password=tiger
 * ```
 *
 * Instead of a database name, Oracle uses a service name which you
 * typically specify in a `tnsnames.ora` configuration file.
 * The auth-part of the URL can be used instead to specify the username
 * and the password as in the example below. Here we also specify that
 * we want to connect to Oracle with the SYSDBA role.
 *
 * ```
 * oracle://sys:password@localhost:1521/servicename?sysdba=true
 * ```
 *
 * See [oracle options](oracleoptions.html) for all properties that
 * can be set for an oracle connection URL.
 *
 * ## Example:
 * To obtain a connection pool for a MySQL database, the code below can be
 * used. The exact same code can be used for PostgreSQL, SQLite and Oracle,
 * the only change needed is to modify the Connection URL. Here we connect
 * to the database test on localhost and start the pool with the default 5
 * initial connections.
 *
 * @code
 * URL_T url = URL_new("mysql://localhost/test?user=root&password=swordfish");
 * ConnectionPool_T pool = ConnectionPool_new(url);
 * ConnectionPool_start(pool);
 * //..
 * Connection_T con = ConnectionPool_getConnection(pool);
 * ResultSet_T result = Connection_executeQuery(con, "select id, name, image from employee where salary>%d", anumber);
 * while (ResultSet_next(result))
 * {
 *      int id = ResultSet_getInt(result, 1);
 *      const char *name = ResultSet_getString(result, 2);
 *      int blobSize;
 *      const void *image = ResultSet_getBlob(result, 3, &blobSize);
 *      // ...
 * }
 * Connection_close(con);
 * //..
 * ConnectionPool_free(&pool);
 * URL_free(&url);
 * @endcode
 *
 * ## Optimizing the pool size:
 * The pool can be set up to dynamically change the number of active
 * Connections in the pool depending on the load. A single `reaper`
 * thread can be activated at startup to sweep through the pool at regular
 * intervals and close Connections that have been inactive for a specified time
 * or for some reason no longer respond. Only inactive Connections will be closed,
 * and no more than the initial number of Connections the pool was started with
 * are closed. The property method, ConnectionPool_setReaper(), is used to specify
 * that a reaper thread should be started when the pool is started. This method
 * **must** be called *before* ConnectionPool_start(); otherwise,
 * the pool will not start with a reaper thread.
 *
 * Clients can also call the method ConnectionPool_reapConnections() to
 * prune the pool directly if the reaper thread is not activated.
 *
 * It is recommended to start the pool with a reaper thread, especially if
 * the pool maintains TCP/IP Connections.
 *
 * ## Realtime inspection:
 * Two methods can be used to inspect the pool at runtime. The method
 * ConnectionPool_size() returns the number of connections in the pool, that is,
 * both active and inactive connections. The method ConnectionPool_active()
 * returns the number of active connections, i.e., those connections in
 * current use by your application.
 *
 * *This ConnectionPool is thread-safe.*
 *
 * @see Connection.h ResultSet.h URL.h PreparedStatement.h SQLException.h
 * @file
 */


#define T ConnectionPool_T
typedef struct ConnectionPool_S *T;


/**
 * Library Debug flag. If set to true, emit debug output
 */
extern int ZBDEBUG;


/**
 * Create a new ConnectionPool. The pool is created with a default of 5
 * initial connections. Maximum connections is set to 20. Property
 * methods in this interface can be used to change the default values.
 *
 * @param url The database connection URL. It is a checked runtime error
 * for the url parameter to be NULL. The pool **does not** take ownership
 * of the `url` object but expects the url to exist as long as the pool does.
 * @return A new ConnectionPool object
 * @see URL.h
 */
T ConnectionPool_new(URL_T url);


/**
 * Disconnect and destroy the pool and release allocated resources.
 *
 * @param P A ConnectionPool object reference
 */
void ConnectionPool_free(T *P);

/** @name Properties */
//@{

/**
 * Returns this Connection Pool's URL
 * 
 *@param P A ConnectionPool object
 * @return This Connection Pool's URL
 * @see URL.h
 */
URL_T ConnectionPool_getURL(T P);


/**
 * Set the number of initial connections to start the pool with
 *
 * @param P A ConnectionPool object
 * @param connections The number of initial pool connections
 * @see Connection.h
 */
void ConnectionPool_setInitialConnections(T P, int connections);


/**
 * Get the number of initial connections to start the pool with
 *
 * @param P A ConnectionPool object
 * @return The number of initial pool connections
 * @see Connection.h
 */
int ConnectionPool_getInitialConnections(T P);


/**
 * Set the maximum number of connections this connection pool will
 * create. If max connections has been reached, ConnectionPool_getConnection()
 * will return NULL on the next call.
 *
 * @param P A ConnectionPool object
 * @param maxConnections The maximum number of connections this
 * connection pool will create. It is a checked runtime error for
 * maxConnections to be less than initialConnections.
 * @see Connection.h
 */
void ConnectionPool_setMaxConnections(T P, int maxConnections);


/**
 * Get the maximum number of connections this Connection pool will create.
 *
 * @param P A ConnectionPool object
 * @return The maximum number of connections this connection pool will create.
 * @see Connection.h
 */
int ConnectionPool_getMaxConnections(T P);


/**
 * Set a Connection inactive timeout value in seconds. The method
 * ConnectionPool_reapConnections(), if called, will close inactive
 * Connections in the pool which have not been in use for
 * `connectionTimeout` seconds. The default connectionTimeout is
 * 30 seconds. The reaper thread, if in use (see ConnectionPool_setReaper()),
 * uses this value when closing inactive Connections. It is a checked runtime
 * error for `connectionTimeout` to be less than or equal to zero.
 *
 * @param P A ConnectionPool object
 * @param connectionTimeout The number of `seconds` a Connection
 * can be inactive (i.e., not in use) before the reaper closes the Connection.
 * (value > 0)
 */
void ConnectionPool_setConnectionTimeout(T P, int connectionTimeout);


/**
 * Returns the connection timeout value in seconds.
 *
 * @param P A ConnectionPool object
 * @return The time an inactive Connection may live before it is closed
 */
int ConnectionPool_getConnectionTimeout(T P);


/**
 * Set the function to call if a fatal error occurs in the library. In
 * practice, this means Out-Of-Memory errors or uncaught exceptions.
 * Clients may optionally provide this function. If not provided,
 * the library will call `abort(3)` upon encountering a
 * fatal error if ZBDEBUG is set; otherwise, exit(1) is called. This
 * method provides clients with a means to close down execution gracefully.
 * It is an unchecked runtime error to continue using the library after
 * the `abortHandler` was called.
 *
 * @param P A ConnectionPool object
 * @param abortHandler The handler function to call should a fatal
 * error occur during processing. An explanatory error message is passed
 * to the handler function in the string `error`
 * @see Exception.h
 */
void ConnectionPool_setAbortHandler(T P, void(*abortHandler)(const char *error));


/**
 * Specify that a reaper thread should be used by the pool. This thread
 * will close all inactive Connections in the pool, down to initial
 * connections. An inactive Connection is closed if and only if its
 * `connectionTimeout` has expired *or* if the Connection
 * failed the ping test. Active Connections, that is, connections in current
 * use by your application, are *never* closed by this thread. This
 * method sets the reaper thread sweep property but does not start the
 * thread. This is done in ConnectionPool_start(). So, if the pool should
 * use a reaper thread, remember to call this method **before**
 * ConnectionPool_start(). It is a checked runtime error for
 * `sweepInterval` to be less than or equal to zero.
 *
 * @param P A ConnectionPool object
 * @param sweepInterval Number of `seconds` between sweeps of the
 * reaper thread (value > 0)
 */
void ConnectionPool_setReaper(T P, int sweepInterval);


//@}

/** @name Functions */
//@{

/**
 * Prepare for the beginning of active use of this component. This method
 * must be called before the pool is used and will connect to the database
 * server and create the initial connections for the pool. This method will
 * also start the reaper thread if specified via ConnectionPool_setReaper().
 *
 * @param P A ConnectionPool object
 * @exception SQLException If a database error occurs.
 * @see SQLException.h
 */
void ConnectionPool_start(T P);


/**
 * Gracefully terminate the active use of the public methods of this
 * component. This method should be the last one called on a given instance
 * of this component. Calling this method closes down all connections in the
 * pool, disconnects the pool from the database server, and stops the reaper
 * thread if it was started.
 *
 * @param P A ConnectionPool object
 */
void ConnectionPool_stop(T P);


/**
 * Get a connection from the pool. The returned Connection (if any) is
 * guaranteed to be alive and connected to the database. NULL is returned
 * if the pool is full and there are no available connections, if a
 * database error occurred, or if the maximum number of retry attempts was reached.
 *
 * The method includes a retry mechanism to handle temporary connection failures,
 * improving robustness in scenarios with network instability.
 *
 * This example can help to differentiate between a full pool (in which case
 * increasing max connections can help) or a database error (in which case nothing
 * will help. Get a coffee instead and call the DBA).
 *
 * ```
 * Connection_T con = ConnectionPool_getConnection(p);
 * if (!con) {
 *      if (ConnectionPool_size(p) == ConnectionPool_active(p)) {
 *          // Pool is full try increasing its max size
 *          ConnectionPool_setMaxConnections(p,
 *              ConnectionPool_getMaxConnections(p) + 5);
 *          con = ConnectionPool_getConnection(p);
 *          // Check if we got a connection
 *      } else {
 *          // A database error occurred or maximum retry attempts were reached
 *          // This could be due to network issues or database unavailability
 *      }
 * }
 * ```
 *
 * @param P A ConnectionPool object
 * @return A connection from the pool or NULL if maxConnection has been
 * reached, if a database error occurred, or if maximum retry attempts were exceeded.
 * @see Connection.h
 * @see ConnectionPool_setMaxRetries(T P, int maxRetries)
 */
Connection_T ConnectionPool_getConnection(T P);

/**
 * Get a connection from the pool or throw an SQL Exception if a connection
 * cannot be obtained. The returned Connection is guaranteed to be
 * alive and connected to the database. The method ConnectionPool_getConnection()
 * above is identical except it will return NULL if the pool is full or
 * if a database error occured. This method will instead throw an SQLException
 * in both cases with an appropriate error message.
 * 
 * @param P A ConnectionPool object
 * @return A connection from the pool
 * @exception SQLException If a database connection cannot be obtained. The
 * error message is available in Exception_frame.message
 * @see Connection.h
 */
Connection_T ConnectionPool_getConnectionOrException(T P);


/**
 * Returns a connection to the pool. This is the same as calling Connection_close()
 * 
 *@param P A ConnectionPool object
 * @param connection A Connection object
 * @see Connection.h
 */
void ConnectionPool_returnConnection(T P, Connection_T connection);


/**
 * Close all inactive Connections in the pool, down to initial connections.
 * An inactive Connection is closed if and only if its
 * `connectionTimeout` has expired *or* if the Connection
 * failed the ping test against the database. Active Connections are
 * *not* closed by this method.
 *
 * @param P A ConnectionPool object
 * @return The number of Connections that were closed
 * @see ConnectionPool_setConnectionTimeout
 * @see ConnectionPool_setInitialConnections
 * @see Connection_ping
 */
int ConnectionPool_reapConnections(T P);


/**
 * Returns the current number of connections in the pool. The number of
 * both active and inactive connections is returned.
 *
 * @param P A ConnectionPool object
 * @return The number of connections in the pool
 */
int ConnectionPool_size(T P);


/**
 * Returns the number of active connections in the pool, i.e., connections
 * in use by clients.
 *
 * @param P A ConnectionPool object
 * @return The number of active connections in the pool
 */
int ConnectionPool_active(T P);

// @}

/** @name Class functions */
//@{

/**
 * **Class method**, returns this library's version information
 *
 * @return Library version information
 */
const char *ConnectionPool_version(void);

// @}

#undef T
#endif

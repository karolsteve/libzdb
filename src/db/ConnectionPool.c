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


#include "Config.h"

#include <stdio.h>
#include <string.h>

#include "URL.h"
#include "Thread.h"
#include "system/Time.h"
#include "Vector.h"
#include "ResultSet.h"
#include "PreparedStatement.h"
#include "Connection.h"
#include "ConnectionPool.h"


/**
 * Implementation of the ConnectionPool interface
 *
 * @file
 */


/* ----------------------------------------------------------- Definitions */


#define T ConnectionPool_T
struct ConnectionPool_S {
        URL_T url;
        bool filled;
        bool doSweep;
        char *error;
        Sem_T alarm;
        Mutex_T mutex;
        Vector_T pool;
        Thread_T reaper;
        int sweepInterval;
        int maxConnections;
        volatile bool stopped;
        int connectionTimeout;
        int initialConnections;
};

int ZBDEBUG = false;
#ifdef PACKAGE_PROTECTED
#pragma GCC visibility push(hidden)
#endif
void(*AbortHandler)(const char *error) = NULL;
#ifdef PACKAGE_PROTECTED
#pragma GCC visibility pop
#endif


/* ------------------------------------------------------- Private methods */


static void _drainPool(T P) {
        while (! Vector_isEmpty(P->pool)) {
                Connection_T con = Vector_pop(P->pool);
                Connection_free(&con);
        }
}


static bool _fillPool(T P) {
        for (int i = 0; i < P->initialConnections; i++) {
                Connection_T con = Connection_new(P, &P->error);
                if (! con) {
                        if (i > 0) {
                                DEBUG("Failed to fill the pool with initial connections -- %s\n", P->error);
                                FREE(P->error);
                                return true;
                        }
                        return false;
                }
                Vector_push(P->pool, con);
        }
        return true;
}


static inline Connection_T _findConnection(T P, int *size) {
        Connection_T con = NULL;
        *size = Vector_size(P->pool);
        for (int i = 0; i < *size; i++) {
                con = Vector_get(P->pool, i);
                if (Connection_isAvailable(con)) {
                        Connection_setAvailable(con, false);
                        return con;
                }
        }
        return NULL;
}


static inline Connection_T _createConnection(T P, char error[static STRLEN]) {
        Connection_T con = Connection_new(P, &P->error);
        if (con) {
                LOCK(P->mutex)
                {
                        Connection_setAvailable(con, false);
                        Vector_push(P->pool, con);
                }
                END_LOCK;
        } else {
                snprintf(error, STRLEN, "Failed to create a connection -- %s",
                         STR_DEF(P->error) ? P->error : "unknown error");
                FREE(P->error);
        }
        return con;
}


static Connection_T _getConnection(T P, char error[static STRLEN]) {
        Connection_T con = NULL;
        int retries = 0;
        const int maxRetries = 10;
        int size = 0;
        *error = 0;
        while (retries < maxRetries) {
                Mutex_lock(P->mutex);
                        con = _findConnection(P, &size);
                Mutex_unlock(P->mutex);
                
                if (!con) {
                        if (size < P->maxConnections) {
                                con = _createConnection(P, error);
                                if (con) return con;
                                else return NULL;  // Don't retry on connection creation failure
                        } else {
                                snprintf(error, STRLEN, "Failed to create a connection -- max connections reached");
                                return NULL;
                        }
                } else {
                        // Invariant: A pool connection must pass the ping-test
                        if (Connection_ping(con)) {
                                return con;
                        } else {
                                Mutex_lock(P->mutex);
                                        Vector_remove(P->pool, Vector_indexOf(P->pool, con));
                                Mutex_unlock(P->mutex);
                                Connection_free(&con);
                        }
                }
                retries++;
        }
        if (retries == maxRetries) {
                snprintf(error, STRLEN, "Failed to get a connection that passed the ping test after %d attempts", maxRetries);
        }
        DEBUG("%s\n", error);
        return NULL;
}


static int _active(T P){
        int i, n = 0, size = Vector_size(P->pool);
        for (i = 0; i < size; i++)
                if (! Connection_isAvailable(Vector_get(P->pool, i))) n++;
        return n;
}


static int _reapConnections(T P) {
        int n = 0;
        int x = Vector_size(P->pool) - _active(P) - P->initialConnections;
        time_t timedout = Time_now() - P->connectionTimeout;
        for (int i = 0; ((n < x) && (i < Vector_size(P->pool))); i++) {
                Connection_T con = Vector_get(P->pool, i);
                if (Connection_isAvailable(con)) {
                        if ((Connection_getLastAccessedTime(con) < timedout) || (! Connection_ping(con))) {
                                Vector_remove(P->pool, i);
                                Connection_free(&con);
                                n++;
                                i--;
                        }
                }
        }
        return n;
}


static void *_doSweep(void *args) {
        T P = args;
        struct timespec wait = {};
        Mutex_lock(P->mutex);
        while (! P->stopped) {
                wait.tv_sec = Time_now() + P->sweepInterval;
                Sem_timeWait(P->alarm,  P->mutex, wait);
                if (P->stopped) break;
                _reapConnections(P);
        }
        Mutex_unlock(P->mutex);
        DEBUG("Reaper thread stopped\n");
        return NULL;
}


/* ---------------------------------------------------------------- Public */


T ConnectionPool_new(URL_T url) {
        T P;
        assert(url);
        System_init();
        NEW(P);
        P->url = url;
        Sem_init(P->alarm);
        Mutex_init(P->mutex);
        P->maxConnections = SQL_DEFAULT_MAX_CONNECTIONS;
        P->pool = Vector_new(SQL_DEFAULT_MAX_CONNECTIONS);
        P->initialConnections = SQL_DEFAULT_INIT_CONNECTIONS;
        P->connectionTimeout = SQL_DEFAULT_CONNECTION_TIMEOUT;
        return P;
}


void ConnectionPool_free(T *P) {
        Vector_T pool;
        assert(P && *P);
        pool = (*P)->pool;
        if (! (*P)->stopped)
                ConnectionPool_stop((*P));
        Vector_free(&pool);
        Mutex_destroy((*P)->mutex);
        Sem_destroy((*P)->alarm);
        FREE((*P)->error);
        FREE(*P);
}


/* ------------------------------------------------------------ Properties */


URL_T ConnectionPool_getURL(T P) {
        assert(P);
        return P->url;
}


void ConnectionPool_setInitialConnections(T P, int connections) {
        assert(P);
        assert(connections >= 0);
        Mutex_lock(P->mutex);
                P->initialConnections = connections;
        Mutex_unlock(P->mutex);
}


int ConnectionPool_getInitialConnections(T P) {
        assert(P);
        return P->initialConnections;
}


void ConnectionPool_setMaxConnections(T P, int maxConnections) {
        assert(P);
        assert(P->initialConnections <= maxConnections);
        Mutex_lock(P->mutex);
                P->maxConnections = maxConnections;
        Mutex_unlock(P->mutex);
}


int ConnectionPool_getMaxConnections(T P) {
        assert(P);
        return P->maxConnections;
}


void ConnectionPool_setConnectionTimeout(T P, int connectionTimeout) {
        assert(P);
        assert(connectionTimeout > 0);
        P->connectionTimeout = connectionTimeout;
}


int ConnectionPool_getConnectionTimeout(T P) {
        assert(P);
        return P->connectionTimeout;
}


void ConnectionPool_setAbortHandler(T P, void(*abortHandler)(const char *error)) {
        assert(P);
        AbortHandler = abortHandler;
}


void ConnectionPool_setReaper(T P, int sweepInterval) {
        assert(P);
        assert(sweepInterval>0);
        LOCK(P->mutex)
        {
                P->doSweep = true;
                P->sweepInterval = sweepInterval;
        }
        END_LOCK;
}


/* -------------------------------------------------------- Public methods */


void ConnectionPool_start(T P) {
        assert(P);
        LOCK(P->mutex)
        {
                P->stopped = false;
                if (! P->filled) {
                        P->filled = _fillPool(P);
                        if (P->filled) {
                                if (P->doSweep) {
                                        DEBUG("Starting Database reaper thread\n");
                                        Thread_create(P->reaper, _doSweep, P);
                                }
                        }
                }
        }
        END_LOCK;
        if (! P->filled)
                THROW(SQLException, "Failed to start connection pool -- %s", P->error);
}


void ConnectionPool_stop(T P) {
        bool stopSweep = false;
        assert(P);
        LOCK(P->mutex)
        {
                P->stopped = true;
                if (P->filled) {
                        _drainPool(P);
                        P->filled = false;
                        stopSweep = (P->doSweep && P->reaper);
                }
        }
        END_LOCK;
        if (stopSweep) {
                DEBUG("Stopping Database reaper thread...\n");
                Sem_signal(P->alarm);
                Thread_join(P->reaper);
        }
}


Connection_T ConnectionPool_getConnection(T P) {
        assert(P);
        return _getConnection(P, (char[STRLEN]){});
}


Connection_T ConnectionPool_getConnectionOrException(T P) {
        assert(P);
        char error[STRLEN] = {};
        Connection_T con = _getConnection(P, error);
        if (!con) {
                THROW(SQLException, "%s", error);
        }
        return con;
}


void ConnectionPool_returnConnection(T P, Connection_T connection) {
        assert(P);
        assert(connection);
        if (Connection_inTransaction(connection)) {
                TRY
                        Connection_rollback(connection);
                ELSE
                        DEBUG("Failed to rollback transaction -- %s\n", Exception_frame.message);
                END_TRY;
        }
        Connection_clear(connection);
        Mutex_lock(P->mutex);
                Connection_setAvailable(connection, true);
        Mutex_unlock(P->mutex);
}


int ConnectionPool_reapConnections(T P) {
        int n = 0;
        assert(P);
        Mutex_lock(P->mutex);
                n = _reapConnections(P);
        Mutex_unlock(P->mutex);
        return n;
}


int ConnectionPool_size(T P) {
        assert(P);
        return Vector_size(P->pool);
}


int ConnectionPool_active(T P) {
        int n = 0;
        assert(P);
        Mutex_lock(P->mutex);
                n = _active(P);
        Mutex_unlock(P->mutex);
        return n;
}


bool ConnectionPool_isFull(T P) {
        assert(P);
        bool full = false;
        Mutex_lock(P->mutex);
                full = Vector_size(P->pool) >= P->maxConnections;
        Mutex_unlock(P->mutex);
        return full;
}


/* --------------------------------------------------------- Class methods */


const char *ConnectionPool_version(void) {
        return ABOUT;
}

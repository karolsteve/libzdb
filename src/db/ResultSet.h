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


#ifndef RESULTSET_INCLUDED
#define RESULTSET_INCLUDED
#include <time.h>
//<< Protected methods
#include "ResultSetDelegate.h"
//>> End Protected methods


/**
 * A **ResultSet** represents a database result set. A ResultSet is
 * created by executing a SQL SELECT statement using either
 * Connection_executeQuery() or PreparedStatement_executeQuery().
 *
 * A ResultSet maintains a cursor pointing to its current row of data.
 * Initially, the cursor is positioned before the first row.
 * ResultSet_next() moves the cursor to the next row, and because
 * it returns false when there are no more rows, it can be used in a while
 * loop to iterate through the result set. A ResultSet is not updatable and
 * has a cursor that moves forward only. Thus, you can iterate through it
 * only once and only from the first row to the last row.
 *
 * The ResultSet interface provides getter methods for retrieving
 * column values from the current row. Values can be retrieved using
 * either the index number of the column or the name of the column. In
 * general, using the column index will be more efficient. *Columns
 * are numbered from 1.*
 *
 * Column names used as input to getter methods are case sensitive.
 * When a getter method is called with a column name and several
 * columns have the same name, the value of the first matching column
 * will be returned. The column name option is designed to be used
 * when column names are used in the SQL query that generated the
 * result set. For columns that are NOT explicitly named in the query,
 * it is best to use column indices.
 *
 * ## Examples
 *
 * The following examples demonstrate how to obtain a ResultSet and
 * how to retrieve values from it.
 *
 * ### Example: Using column names
 *
 * In this example, columns are named in the SELECT statement, and we retrieve
 * values using the column names (we could of course also use indices if we
 * want):
 *
 * @code
 * ResultSet_T r = Connection_executeQuery(con, "SELECT ssn, name, photo FROM employees");
 * while (ResultSet_next(r)) 
 * {
 *     int ssn = ResultSet_getIntByName(r, "ssn");
 *     const char *name = ResultSet_getStringByName(r, "name");
 *     int photoSize;
 *     const void *photo = ResultSet_getBlobByName(r, "photo", &photoSize);
 *     if (photoSize > 0) 
 *     {
 *         // Process photo data
 *     }
 *     // Process other data...
 * }
 * @endcode
 *
 * ### Example: Using column indices
 *
 * This example demonstrates selecting a generated result and printing it.
 * When the SELECT statement doesn't name the column, we use the column
 * index to retrieve the value:
 *
 * @code
 * ResultSet_T r = Connection_executeQuery(con, "SELECT COUNT(*) FROM employees");
 * if (ResultSet_next(r)) 
 * {
 *     const char *count = ResultSet_getString(r, 1);
 *     printf("Number of employees: %s\n", count ? count : "none");
 * } 
 * else
 * {
 *     printf("No results returned\n");
 * }
 * @endcode
 *
 * ## Automatic type conversions
 *
 * A ResultSet stores values internally as bytes and converts values
 * on-the-fly to numeric types when requested, such as when ResultSet_getInt()
 * or one of the other numeric get-methods are called. In the above example,
 * even if *count(\*)* returns a numeric value, we can use
 * ResultSet_getString() to get the number as a string or if we choose, we can use
 * ResultSet_getInt() to get the value as an integer. In the latter case, note
 * that if the column value cannot be converted to a number, an SQLException is thrown.
 *
 * ## Date and Time
 *
 * ResultSet provides two principal methods for retrieving temporal column
 * values as C types. ResultSet_getTimestamp() converts a SQL timestamp value
 * to a `time_t` and ResultSet_getDateTime() returns a
 * `tm structure` representing a SQL Date, Time, DateTime, or Timestamp
 * column type. To get a temporal column value as a string, simply use ResultSet_getString()
 *
 * *A ResultSet is reentrant, but not thread-safe and should only be used by
 * one thread (at a time).*
 *
 * @note Remember that column indices in ResultSet are 1-based, not 0-based.
 *
 * @see Connection.h PreparedStatement.h SQLException.h
 * @file
 */


#define T ResultSet_T
typedef struct ResultSet_S *T;


//<< Protected methods

/**
 * Create a new ResultSet.
 * @param D the delegate used by this ResultSet
 * @param op delegate operations
 * @return A new ResultSet object
 */
T ResultSet_new(ResultSetDelegate_T D, Rop_T op) __attribute__ ((visibility("hidden")));


/**
 * Destroy a ResultSet and release allocated resources.
 * @param R A ResultSet object reference
 */
void ResultSet_free(T *R) __attribute__ ((visibility("hidden")));

//>> End Protected methods

/** @name Properties */
//@{

/**
 * Returns the number of columns in this ResultSet object.
 * @param R A ResultSet object
 * @return The number of columns
 */
int ResultSet_getColumnCount(T R);


/**
 * Get the designated column's name.
 * @param R A ResultSet object
 * @param columnIndex The first column is 1, the second is 2, ...
 * @return Column name or NULL if the column does not exist. You
 * should use the method ResultSet_getColumnCount() to test for
 * the availability of columns in the result set.
 */
const char *ResultSet_getColumnName(T R, int columnIndex);


/**
 * Returns column size in bytes. If the column is a blob then
 * this method returns the number of bytes in that blob. No type
 * conversions occur. If the result is a string (or a number
 * since a number can be converted into a string) then return the
 * number of bytes in the resulting string.
 * @param R A ResultSet object
 * @param columnIndex The first column is 1, the second is 2, ...
 * @return Column data size
 * @exception SQLException If columnIndex is outside the valid range
 * @see SQLException.h
 */
long ResultSet_getColumnSize(T R, int columnIndex);


/**
 * Specify the number of rows that should be fetched from the database
 * when more rows are needed for **this** ResultSet. ResultSet will prefetch
 * rows in batches of number of `rows` when ResultSet_next()
 * is called to reduce the network roundtrip to the database. This method
 * is only applicable to MySQL and Oracle.
 * @param R A ResultSet object
 * @param rows The number of rows to fetch (1..INT_MAX)
 * @exception SQLException If a database error occurs
 * @exception AssertException If `rows` is less than 1
 * @see Connection_setFetchSize
 */
void ResultSet_setFetchSize(T R, int rows);


/**
 * Get the number of rows that should be fetched from the database
 * when more rows are needed for this ResultSet. Unless previously set
 * with ResultSet_setFetchSize(), the returned value is the same as
 * returned by Connection_getFetchSize()
 * @param R A ResultSet object
 * @return The number of rows to fetch or 0 if N/A
 * @see Connection_getFetchSize
 */
int ResultSet_getFetchSize(T R);


//@}

/** @name Functions */
//@{

/**
 * Moves the cursor down one row from its current position. A
 * ResultSet cursor is initially positioned before the first row; the
 * first call to this method makes the first row the current row; the
 * second call makes the second row the current row, and so on. When
 * there are no more available rows false is returned. An empty
 * ResultSet will return false on the first call to ResultSet_next().
 * @param R A ResultSet object
 * @return true if the new current row is valid; false if there are no
 * more rows
 * @exception SQLException If a database access error occurs
 */
bool ResultSet_next(T R);


//@}

/** @name Columns */
//@{

/**
 * Returns true if the value of the designated column in the current row of
 * this ResultSet object is SQL NULL, otherwise false. If the column value is
 * SQL NULL, a ResultSet returns the NULL pointer for reference types and 0
 * for value types. Use this method if you need to differentiate between SQL NULL and
 * the value NULL/0.
 * @param R A ResultSet object
 * @param columnIndex The first column is 1, the second is 2, ...
 * @return True if column value is SQL NULL, otherwise false
 * @exception SQLException If a database access error occurs or
 * columnIndex is outside the valid range
 * @see SQLException.h
 */
bool ResultSet_isnull(T R, int columnIndex);


/**
 * Retrieves the value of the designated column in the current row of
 * this ResultSet object as a C-string. If `columnIndex`
 * is outside the range [1..ResultSet_getColumnCount()] this
 * method throws an SQLException. *The returned string may only be
 * valid until the next call to ResultSet_next() and if you plan to use
 * the returned value longer, you must make a copy.*
 * @param R A ResultSet object
 * @param columnIndex The first column is 1, the second is 2, ...
 * @return The column value; if the value is SQL NULL, the value
 * returned is NULL
 * @exception SQLException If a database access error occurs or
 * columnIndex is outside the valid range
 * @see SQLException.h
 */
const char *ResultSet_getString(T R, int columnIndex);


/**
 * Retrieves the value of the designated column in the current row of
 * this ResultSet object as a C-string. If `columnName`
 * is not found this method throws an SQLException. *The returned string
 * may only be valid until the next call to ResultSet_next() and if you plan
 * to use the returned value longer, you must make a copy.*
 * @param R A ResultSet object
 * @param columnName The SQL name of the column. *case-sensitive*
 * @return The column value; if the value is SQL NULL, the value
 * returned is NULL
 * @exception SQLException If a database access error occurs or
 * columnName does not exist
 * @see SQLException.h
 */
const char *ResultSet_getStringByName(T R, const char *columnName);


/**
 * Retrieves the value of the designated column in the current row of this
 * ResultSet object as an int. If `columnIndex` is outside the
 * range [1..ResultSet_getColumnCount()] this method throws an SQLException.
 * In general, on both 32 and 64 bit architectures, `int` is 4 bytes
 * or 32 bits and `long long` is 8 bytes or 64 bits. A
 * `long` type is usually equal to `int` on 32 bit
 * architecture and equal to `long long` on 64 bit architecture.
 * However, the width of integer types is architecture and compiler dependent.
 * The above is usually true, but not necessarily.
 * @param R A ResultSet object
 * @param columnIndex The first column is 1, the second is 2, ...
 * @return The column value; if the value is SQL NULL, the value
 * returned is 0
 * @exception SQLException If a database access error occurs, columnIndex
 * is outside the valid range or if the value is NaN
 * @see SQLException.h
 */
int ResultSet_getInt(T R, int columnIndex);


/**
 * Retrieves the value of the designated column in the current row of
 * this ResultSet object as an int. If `columnName` is
 * not found this method throws an SQLException.
 * In general, on both 32 and 64 bit architectures, `int` is 4 bytes
 * or 32 bits and `long long` is 8 bytes or 64 bits. A
 * `long` type is usually equal to `int` on 32 bit
 * architecture and equal to `long long` on 64 bit architecture.
 * However, the width of integer types is architecture and compiler dependent.
 * The above is usually true, but not necessarily.
 * @param R A ResultSet object
 * @param columnName The SQL name of the column. *case-sensitive*
 * @return The column value; if the value is SQL NULL, the value
 * returned is 0
 * @exception SQLException If a database access error occurs, columnName
 * does not exist or if the value is NaN
 * @see SQLException.h
 */
int ResultSet_getIntByName(T R, const char *columnName);


/**
 * Retrieves the value of the designated column in the current row of
 * this ResultSet object as a long long. If `columnIndex`
 * is outside the range [1..ResultSet_getColumnCount()] this
 * method throws an SQLException.
 * In general, on both 32 and 64 bit architectures, `int` is 4 bytes
 * or 32 bits and `long long` is 8 bytes or 64 bits. A
 * `long` type is usually equal to `int` on 32 bit
 * architecture and equal to `long long` on 64 bit architecture.
 * However, the width of integer types is architecture and compiler dependent.
 * The above is usually true, but not necessarily.
 * @param R A ResultSet object
 * @param columnIndex The first column is 1, the second is 2, ...
 * @return The column value; if the value is SQL NULL, the value
 * returned is 0
 * @exception SQLException If a database access error occurs,
 * columnIndex is outside the valid range or if the value is NaN
 * @see SQLException.h
 */
long long ResultSet_getLLong(T R, int columnIndex);


/**
 * Retrieves the value of the designated column in the current row of
 * this ResultSet object as a long long. If `columnName`
 * is not found this method throws an SQLException.
 * In general, on both 32 and 64 bit architectures, `int` is 4 bytes
 * or 32 bits and `long long` is 8 bytes or 64 bits. A
 * `long` type is usually equal to `int` on 32 bit
 * architecture and equal to `long long` on 64 bit architecture.
 * However, the width of integer types is architecture and compiler dependent.
 * The above is usually true, but not necessarily.
 * @param R A ResultSet object
 * @param columnName The SQL name of the column. *case-sensitive*
 * @return The column value; if the value is SQL NULL, the value
 * returned is 0
 * @exception SQLException If a database access error occurs, columnName
 * does not exist or if the value is NaN
 * @see SQLException.h
 */
long long ResultSet_getLLongByName(T R, const char *columnName);


/**
 * Retrieves the value of the designated column in the current row of
 * this ResultSet object as a double. If `columnIndex`
 * is outside the range [1..ResultSet_getColumnCount()] this
 * method throws an SQLException.
 * @param R A ResultSet object
 * @param columnIndex The first column is 1, the second is 2, ...
 * @return The column value; if the value is SQL NULL, the value
 * returned is 0.0
 * @exception SQLException If a database access error occurs, columnIndex
 * is outside the valid range or if the value is NaN
 * @see SQLException.h
 */
double ResultSet_getDouble(T R, int columnIndex);


/**
 * Retrieves the value of the designated column in the current row of
 * this ResultSet object as a double. If `columnName` is
 * not found this method throws an SQLException.
 * @param R A ResultSet object
 * @param columnName The SQL name of the column. *case-sensitive*
 * @return The column value; if the value is SQL NULL, the value
 * returned is 0.0
 * @exception SQLException If a database access error occurs, columnName
 * does not exist or if the value is NaN
 * @see SQLException.h
 */
double ResultSet_getDoubleByName(T R, const char *columnName);


/**
 * Retrieves the value of the designated column in the current row of
 * this ResultSet object as a void pointer. If `columnIndex`
 * is outside the range [1..ResultSet_getColumnCount()] this method
 * throws an SQLException. *The returned blob may only be valid until
 * the next call to ResultSet_next() and if you plan to use the returned
 * value longer, you must make a copy.*
 * @param R A ResultSet object
 * @param columnIndex The first column is 1, the second is 2, ...
 * @param size The number of bytes in the blob is stored in size
 * @return The column value; if the value is SQL NULL, the value
 * returned is NULL
 * @exception SQLException If a database access error occurs or
 * columnIndex is outside the valid range
 * @see SQLException.h
 */
const void *ResultSet_getBlob(T R, int columnIndex, int *size);


/**
 * Retrieves the value of the designated column in the current row of
 * this ResultSet object as a void pointer. If `columnName`
 * is not found this method throws an SQLException. *The returned
 * blob may only be valid until the next call to ResultSet_next() and if
 * you plan to use the returned value longer, you must make a copy.*
 * @param R A ResultSet object
 * @param columnName The SQL name of the column. *case-sensitive*
 * @param size The number of bytes in the blob is stored in size
 * @return The column value; if the value is SQL NULL, the value
 * returned is NULL
 * @exception SQLException If a database access error occurs or
 * columnName does not exist
 * @see SQLException.h
 */
const void *ResultSet_getBlobByName(T R, const char *columnName, int *size);

//@}

/** @name Date and Time */
//@{

/**
 * Retrieves the value of the designated column in the current row of this
 * ResultSet object as a Unix timestamp. The returned value is in Coordinated
 * Universal Time (UTC) and represents seconds since the **epoch**
 * (January 1, 1970, 00:00:00 GMT).
 *
 * Even though the underlying database might support timestamp ranges before
 * the epoch and after '2038-01-19 03:14:07 UTC' it is safest not to assume or
 * use values outside this range. Especially on a 32-bit system.
 *
 * *SQLite* does not have temporal SQL data types per se
 * and using this method with SQLite assumes the column value in the Result Set
 * to be either a numerical value representing a Unix Time in UTC which is
 * returned as-is or an [ISO 8601](http://en.wikipedia.org/wiki/ISO_8601)
 * time string which is converted to a time_t value.
 * See also PreparedStatement_setTimestamp()
 * @param R A ResultSet object
 * @param columnIndex The first column is 1, the second is 2, ...
 * @return The column value as seconds since the epoch in the
 * *GMT timezone*. If the value is SQL NULL, the
 * value returned is 0, i.e. January 1, 1970, 00:00:00 GMT
 * @exception SQLException If a database access error occurs, if
 * `columnIndex` is outside the range [1..ResultSet_getColumnCount()]
 * or if the column value cannot be converted to a valid timestamp
 * @see SQLException.h PreparedStatement_setTimestamp
 */
time_t ResultSet_getTimestamp(T R, int columnIndex);


/**
 * Retrieves the value of the designated column in the current row of this
 * ResultSet object as a Unix timestamp. The returned value is in Coordinated
 * Universal Time (UTC) and represents seconds since the **epoch**
 * (January 1, 1970, 00:00:00 GMT).
 *
 * Even though the underlying database might support timestamp ranges before
 * the epoch and after '2038-01-19 03:14:07 UTC' it is safest not to assume or
 * use values outside this range. Especially on a 32-bit system.
 *
 * *SQLite* does not have temporal SQL data types per se
 * and using this method with SQLite assumes the column value in the Result Set
 * to be either a numerical value representing a Unix Time in UTC which is
 * returned as-is or an [ISO 8601](http://en.wikipedia.org/wiki/ISO_8601)
 * time string which is converted to a time_t value.
 * See also PreparedStatement_setTimestamp()
 * @param R A ResultSet object
 * @param columnName The SQL name of the column. *case-sensitive*
 * @return The column value as seconds since the epoch in the
 * *GMT timezone*. If the value is SQL NULL, the
 * value returned is 0, i.e. January 1, 1970, 00:00:00 GMT
 * @exception SQLException If a database access error occurs, if
 * `columnName` is not found or if the column value cannot be
 * converted to a valid timestamp
 * @see SQLException.h PreparedStatement_setTimestamp
 */
time_t ResultSet_getTimestampByName(T R, const char *columnName);


/**
 * Retrieves the value of the designated column in the current row of this
 * ResultSet object as a Date, Time or DateTime. This method can be used to
 * retrieve the value of columns with the SQL data type, Date, Time, DateTime
 * or Timestamp. The returned `tm` structure follows the convention
 * for usage with mktime(3) where:
 *
 * - tm_hour = hours since midnight [0-23]
 * - tm_min = minutes after the hour [0-59]
 * - tm_sec = seconds after the minute [0-60]
 * - tm_mday = day of the month [1-31]
 * - tm_mon = months since January **[0-11]**
 *
 * If the column value contains timezone information, tm_gmtoff is set to the
 * offset from UTC in seconds, otherwise tm_gmtoff is set to 0. *On systems
 * without tm_gmtoff, (Solaris), the member, tm_wday is set to gmt offset
 * instead as this property is ignored by mktime on input.* The exception to
 * the above is **tm_year** which contains the year literal and *not years
 * since 1900* which is the convention. All other fields in the structure are
 * set to zero. If the column type is DateTime or Timestamp all the fields
 * mentioned above are set, if it is a Date or a Time, only the relevant
 * fields are set.
 *
 * @param R A ResultSet object
 * @param columnIndex The first column is 1, the second is 2, ...
 * @return A tm structure with fields for date and time. If the value is SQL
 * NULL, a zeroed tm structure is returned. Use ResultSet_isnull() if in doubt.
 * @exception SQLException If a database access error occurs, if
 * `columnIndex` is outside the range [1..ResultSet_getColumnCount()]
 * or if the column value cannot be converted to a valid SQL Date, Time or
 * DateTime type
 * @see SQLException.h
 */
struct tm ResultSet_getDateTime(T R, int columnIndex);


/**
 * Retrieves the value of the designated column in the current row of this
 * ResultSet object as a Date, Time or DateTime. This method can be used to
 * retrieve the value of columns with the SQL data type, Date, Time, DateTime
 * or Timestamp. The returned `tm` structure follows the convention
 * for usage with mktime(3) where:
 *
 * - tm_hour = hours since midnight [0-23]
 * - tm_min = minutes after the hour [0-59]
 * - tm_sec = seconds after the minute [0-60]
 * - tm_mday = day of the month [1-31]
 * - tm_mon = months since January **[0-11]**
 *
 * If the column value contains timezone information, tm_gmtoff is set to the
 * offset from UTC in seconds, otherwise tm_gmtoff is set to 0. *On systems
 * without tm_gmtoff, (Solaris), the member, tm_wday is set to gmt offset
 * instead as this property is ignored by mktime on input.* The exception to
 * the above is **tm_year** which contains the year literal and *not years
 * since 1900* which is the convention. All other fields in the structure are
 * set to zero. If the column type is DateTime or Timestamp all the fields
 * mentioned above are set, if it is a Date or a Time, only the relevant
 * fields are set.
 *
 * @param R A ResultSet object
 * @param columnName The SQL name of the column. *case-sensitive*
 * @return A tm structure with fields for date and time. If the value is SQL
 * NULL, a zeroed tm structure is returned. Use ResultSet_isnull() if in doubt.
 * @exception SQLException If a database access error occurs, if
 * `columnName` is not found or if the column value cannot be
 * converted to a valid SQL Date, Time or DateTime type
 * @see SQLException.h
 */
struct tm ResultSet_getDateTimeByName(T R, const char *columnName);

//@}

#undef T
#endif

/*
 * Copyright (C) 2019-2024 Tildeslash Ltd.
 * Copyright (C) 2016 dragon jiang<jianlinlong@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files(the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions :
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _ZDBPP_H_
#define _ZDBPP_H_

#include "zdb.h"
#include <string>
#include <utility>
#include <stdexcept>
#include <ctime>
#include <type_traits>
#include <cstdint>
#include <optional>
#include <memory>
#include <span>
#include <variant>

/*
# zdbpp.h - C++ Interface for libzdb
 
 Libzdb as a small, easy to use Open Source Database Connection Pool Library
 with the following features:
 
 - Thread safe Database Connection Pool
 - Connect to multiple database systems simultaneously
 - Zero runtime configuration, connect using a URL scheme
 - Supports MySQL, PostgreSQL, SQLite and Oracle

This interface provides a C++ wrapper for libzdb and offers a convenient and
type-safe way to interact with various SQL databases from C++ applications.

## Features
 
 - Object-oriented wrapper around libzdb
 - Exception-based error handling
 - RAII-compliant resource management
 - Type-safe parameter binding for prepared statements
 - Connection pooling
 - Thread-safe design
 - Modern C++ features (C++20 or later required)
 
## Usage

To use libzdb in your C++ project, include zdbpp.h and use the `zdb` namespace:

```cpp
 #include <zdbpp.h>
 using namespace zdb;
```

## Examples

### Query Execution

```cpp
 ConnectionPool pool("mysql://192.168.11.100:3306/test?user=root&password=dba");
 pool.start();
 Connection con = pool.getConnection();
 
 // Use C++ variadic template feature to bind parameters
 ResultSet result = con.executeQuery(
     "SELECT id, name, hired, image FROM employee WHERE id < ? ORDER BY id", 100
 );
 
 // Optionally set row prefetch, default is 100
 result.setFetchSize(10);
 
 while (result.next()) {
     int id = result.getInt("id");
     auto name = result.getString("name").value_or("N/A");
     auto hired = result.getTimestamp("hired");
     auto blob = result.getBlob("image");
     if (blob) {
         // Process image data...
     }
     // ...
 }
```

### Execute Statement

```cpp
Connection con = pool.getConnection();
// Any execute or executeQuery statement which takes parameters are
// automatically translated into a prepared statement. Here we also
// demonstrate how to set a SQL null value by using nullptr
con.execute("UPDATE employee SET image = ? WHERE id = ?", nullptr, 11);
```

### Test for SQL NULL Value

```cpp
ResultSet result = con.executeQuery("SELECT name, image FROM employee");
while (result.next()) {
    if (result.isNull("image")) {
        // Handle NULL image...
    }
}
```

### Insert Data via Prepared Statement

```cpp
Connection con = pool.getConnection();
PreparedStatement prep = con.prepareStatement(
    "INSERT INTO employee (name, hired, image) VALUES (?, ?, ?)"
);

con.beginTransaction();
for (const auto &employee : employees) {
    // Polymorphic bind
    prep.bind(1, employee.name);
    prep.bind(2, employee.hired);
    prep.bind(3, std::make_tuple(employee.image.data(), employee.image.size()));
    prep.execute();
}
con.commit();
```

### Exception Handling

```cpp
try {
    Connection con = pool.getConnection();
    con.executeQuery("INVALID QUERY");
} catch (const sql_exception& e) {
    std::cout << "SQL error: " << e.what() << std::endl;
}
```

## Key Classes

- `URL`: Represents a database connection URL
- `ResultSet`: Represents a database result set
- `PreparedStatement`: Represents a pre-compiled SQL statement
- `Connection`: Represents a database connection
- `ConnectionPool`: Manages a pool of database connections

For detailed API documentation, refer to the comments for each class in this header file.
Visit [libzd's homepage](https://www.tildeslash.com/libzdb/) for documentation and examples.
*/

namespace zdb {
    namespace {
        #define except_wrapper(f) TRY { f; } ELSE { throw sql_exception(Exception_frame.message); } END_TRY
        
        constexpr std::optional<std::string_view> _to_optional(const char* str) noexcept {
            return str ? std::optional<std::string_view>{str} : std::nullopt;
        }
        
        std::function<void(std::string_view)> g_abortHandler;
        
        void bridgeAbortHandler(const char* error) {
            if (g_abortHandler) {
                g_abortHandler(error);
            }
        }

        struct noncopyable {
            noncopyable() = default;
            noncopyable(const noncopyable&) = delete;
            noncopyable& operator=(const noncopyable&) = delete;
            noncopyable(noncopyable&&) = delete;
            noncopyable& operator=(noncopyable&&) = delete;
        };
    } // anonymous namespace
    
    
    /**
     * @class sql_exception
     * @brief Exception class for SQL related errors.
     *
     * Thrown for SQL errors. Inherits from `std::runtime_error`.
     *
     * Example:
     * @code
     * try {
     *     con.executeQuery("invalid query");
     * } catch (const zdb::sql_exception& e) {
     *     std::cout << "SQL error: " << e.what() << std::endl;
     * }
     * @endcode
     */
    class sql_exception : public std::runtime_error {
    public:
        /**
         * @brief Constructs a new sql_exception with an optional error message.
         * @param msg A C-string representing the error message. Defaults to "SQLException".
         */
        explicit sql_exception(const char* msg = "SQLException") : std::runtime_error(msg) {}
    };

    
    /**
     * @class URL
     * @brief Represents an immutable Uniform Resource Locator.
     *
     * Example: `http://user:password@www.foo.bar:8080/document/index.csp?querystring#ref`
     * Supports IPv6 as defined in RFC2732.
     */
    class URL {
    public:
        /**
         * @brief Create a new URL object from the URL parameter string.
         * @param url A string specifying the URL.
         * @throws sql_exception if the URL cannot be parsed.
         */
        explicit URL(const std::string& url) : t_(URL_new(url.c_str())) {
            if (!t_) throw sql_exception("Invalid URL");
        }
        
        /**
         * @brief Copy constructor.
         * @param r URL object to copy.
         */
        URL(const URL& r) : t_(URL_new(URL_toString(r.t_))) {
            if (!t_) throw sql_exception("Failed to copy URL");
        }
        
        /**
         * @brief Move constructor.
         * @param r URL object to move.
         */
        URL(URL&& r) noexcept : t_(r.t_) {
            r.t_ = nullptr;
        }
        
        /**
         * @brief Copy assignment operator.
         * @param r URL object to copy assign.
         */
        URL& operator=(const URL& r) {
            if (this != &r) {
                URL tmp(r);
                std::swap(t_, tmp.t_);
            }
            return *this;
        }
        
        /**
         * @brief Move assignment operator.
         * @param r URL object to move assign.
         */
        URL& operator=(URL&& r) noexcept {
            if (this != &r) {
                if (t_) URL_free(&t_);
                t_ = r.t_;
                r.t_ = nullptr;
            }
            return *this;
        }

        /**
         * @brief Destroy the URL object.
         */
        ~URL() { if (t_) URL_free(&t_); }
        
        /**
         * @brief Get the protocol of the URL.
         * @return The protocol name.
         */
        [[nodiscard]] constexpr std::string_view protocol() const noexcept { return URL_getProtocol(t_); }
        
        /**
         * @brief Get the user name from the authority part of the URL.
         * @return An optional containing the username specified in the URL,
         * or std::nullopt if not found.
         */
        [[nodiscard]] constexpr std::optional<std::string_view> user() const noexcept { return _to_optional(URL_getUser(t_)); }
        
        /**
         * @brief Get the password from the authority part of the URL.
         * @return An optional containing the password specified in the URL,
         * or std::nullopt if not found.
         */
        [[nodiscard]] constexpr std::optional<std::string_view> password() const noexcept { return _to_optional(URL_getPassword(t_)); }
        
        /**
         * @brief Get the hostname of the URL.
         * @return An optional containing the hostname specified in the URL,
         * or std::nullopt if not found.
         */
        [[nodiscard]] constexpr std::optional<std::string_view> host() const noexcept { return _to_optional(URL_getHost(t_)); }
        
        /**
         * @brief Get the port of the URL.
         * @return The port number of the URL or -1 if not specified.
         */
        [[nodiscard]] constexpr int port() const noexcept { return URL_getPort(t_); }
        
        /**
         * @brief Get the path of the URL.
         * @return An optional containing the path of the URL,
         * or std::nullopt if not found.
         */
        [[nodiscard]] constexpr std::optional<std::string_view> path() const noexcept { return _to_optional(URL_getPath(t_)); }
        
        /**
         * @brief Get the query string of the URL.
         * @return An optional containing the query string of the URL,
         * or std::nullopt if not found.
         */
        [[nodiscard]] constexpr std::optional<std::string_view> queryString() const noexcept { return _to_optional(URL_getQueryString(t_)); }
        
        /**
         * @brief Returns a string representation of this URL object.
         * @return The URL string.
         */
        [[nodiscard]] constexpr std::string_view toString() const noexcept { return URL_toString(t_); }
        
        /**
         * @brief Returns a vector of string objects with the names of the
         * parameters contained in this URL.
         * @return A vector of strings, each string containing the name of
         * a URL parameter; or an empty vector if the URL has no parameters.
         */
        [[nodiscard]] std::vector<std::string_view> parameterNames() const noexcept {
            std::vector<std::string_view> names;
            const char **rawNames = URL_getParameterNames(t_);
            if (rawNames) {
                for (int i = 0; rawNames[i] != nullptr; i++) {
                    names.emplace_back(rawNames[i]);
                }
            }
            return names;
        }
        
        /**
         * @brief Get the value of the specified URL parameter.
         * @param name The parameter name to lookup.
         * @return An optional containing the parameter value, or std::nullopt if not found.
         */
        [[nodiscard]] std::optional<std::string_view> parameter(const std::string& name) const noexcept {
            return _to_optional(URL_getParameter(t_, name.c_str()));
        }
        
        /**
         * @brief Cast operator to URL_T.
         * @return The internal URL_T representation.
         */
        operator URL_T() const noexcept { return t_; }
        
    private:
        URL_T t_;
    };

    
    /**
     * @class ResultSet
     * @brief Represents a database result set.
     *
     * Created by executing a SQL SELECT statement. Maintains a cursor pointing to its current row of data.
     * Example:
     * @code
     * ResultSet result = con.executeQuery("SELECT ssn, name, picture FROM employees");
     * while (result.next()) {
     *     int ssn = result.getInt("ssn");
     *     auto name = result.getString("name").value_or("null");
     *     auto blob = result.getBlob("picture");
     *     if (blob) {
     *          // Use the blob...
     *     } else {
     *          // Handle the case where the blob is not found...
     *     }
     *     // ...
     * }
     * @endcode
     */
    class ResultSet : private noncopyable {
    public:
        /**
         * @brief Move constructor.
         * @param r ResultSet object to move.
         */
        ResultSet(ResultSet&& r) noexcept : t_(r.t_) { r.t_ = nullptr; }
        
        /**
         * @brief Conversion operator to ResultSet_T.
         * @return The underlying ResultSet_T.
         */
        operator ResultSet_T() noexcept { return t_; }
        
        /**
         * @brief Returns the number of columns in this ResultSet object.
         * @return The number of columns.
         */
        [[nodiscard]] int columnCount() const noexcept { return ResultSet_getColumnCount(t_); }
        
        /**
         * @brief Get the designated column's name.
         * @param columnIndex The first column is 1, the second is 2, ...
         * @return An optional containing the Column name, or std::nullopt if the column does not exist.
         */
        [[nodiscard]] std::optional<std::string_view> columnName(int columnIndex) const noexcept {
            return _to_optional(ResultSet_getColumnName(t_, columnIndex));
        }
        
        /**
         * @brief Returns column size in bytes.
         * @param columnIndex The first column is 1, the second is 2, ...
         * @return Column data size.
         * @throws sql_exception If columnIndex is outside the valid range.
         */
        [[nodiscard]] long columnSize(int columnIndex) {
            except_wrapper(RETURN ResultSet_getColumnSize(t_, columnIndex));
        }
        
        /**
         * @brief Specify the number of rows that should be fetched from the database when more rows are needed.
         * @param prefetch_rows The number of rows to fetch (1..INT_MAX).
         */
        void setFetchSize(int rows) noexcept { ResultSet_setFetchSize(t_, rows); }
        
        /**
         * @brief Get the number of rows that should be fetched from the database when more rows are needed.
         * @return The number of rows to fetch or 0 if N/A.
         */
        [[nodiscard]] int getFetchSize() const noexcept { return ResultSet_getFetchSize(t_); }
        
        /**
         * @brief Moves the cursor down one row from its current position.
         * @return true if the new current row is valid; false if there are no more rows.
         * @throws sql_exception If a database access error occurs.
         */
        bool next() { except_wrapper(RETURN ResultSet_next(t_)); }
        
        /**
         * @brief Returns true if the value of the designated column in the current row is SQL NULL, otherwise false.
         * @param columnIndex The first column is 1, the second is 2, ...
         * @return True if column value is SQL NULL, otherwise false.
         * @throws sql_exception If a database access error occurs or columnIndex is outside the valid range.
         */
        [[nodiscard]] bool isNull(int columnIndex) {
            except_wrapper(RETURN ResultSet_isnull(t_, columnIndex));
        }
        
        /**
         * @brief Retrieves the value of the designated column as a string.
         * @param columnIndex The first column is 1, the second is 2, ...
         * @return An optional containing the column value, or std::nullopt if NULL.
         * @throws sql_exception If a database access error occurs or columnIndex is outside the valid range.
         */
        [[nodiscard]] std::optional<std::string_view> getString(int columnIndex) {
            except_wrapper(RETURN _to_optional(ResultSet_getString(t_, columnIndex)));
        }
        
        /**
         * @brief Retrieves the value of the designated column as a string.
         * @param columnName The SQL name of the column. case-sensitive.
         * @return An optional containing the column value, or std::nullopt if NULL.
         * @throws sql_exception If a database access error occurs or columnName does not exist.
         */
        [[nodiscard]] std::optional<std::string_view> getString(const std::string& columnName) {
            except_wrapper(RETURN _to_optional(ResultSet_getStringByName(t_, columnName.c_str())));
        }
        
        /**
         * @brief Retrieves the value of the designated column as an int.
         * @param columnIndex The first column is 1, the second is 2, ...
         * @return The column value; if the value is SQL NULL, the value returned is 0.
         * @throws sql_exception If a database access error occurs, columnIndex is outside the valid range or if the value is NaN.
         */
        [[nodiscard]] int getInt(int columnIndex) {
            except_wrapper(RETURN ResultSet_getInt(t_, columnIndex));
        }
        
        /**
         * @brief Retrieves the value of the designated column as an int.
         * @param columnName The SQL name of the column. case-sensitive.
         * @return The column value; if the value is SQL NULL, the value returned is 0.
         * @throws sql_exception If a database access error occurs, columnName does not exist or if the value is NaN.
         */
        [[nodiscard]] int getInt(const std::string& columnName) {
            except_wrapper(RETURN ResultSet_getIntByName(t_, columnName.c_str()));
        }
        
        /**
         * @brief Retrieves the value of the designated column as a long long.
         * @param columnIndex The first column is 1, the second is 2, ...
         * @return The column value; if the value is SQL NULL, the value returned is 0.
         * @throws sql_exception If a database access error occurs, columnIndex is outside the valid range or if the value is NaN.
         */
        [[nodiscard]] long long getLLong(int columnIndex) {
            except_wrapper(RETURN ResultSet_getLLong(t_, columnIndex));
        }
        
        /**
         * @brief Retrieves the value of the designated column as a long long.
         * @param columnName The SQL name of the column. case-sensitive.
         * @return The column value; if the value is SQL NULL, the value returned is 0.
         * @throws sql_exception If a database access error occurs, columnName does not exist or if the value is NaN.
         */
        [[nodiscard]] long long getLLong(const std::string& columnName) {
            except_wrapper(RETURN ResultSet_getLLongByName(t_, columnName.c_str()));
        }
        
        /**
         * @brief Retrieves the value of the designated column as a double.
         * @param columnIndex The first column is 1, the second is 2, ...
         * @return The column value; if the value is SQL NULL, the value returned is 0.0.
         * @throws sql_exception If a database access error occurs, columnIndex is outside the valid range or if the value is NaN.
         */
        [[nodiscard]] double getDouble(int columnIndex) {
            except_wrapper(RETURN ResultSet_getDouble(t_, columnIndex));
        }
        
        /**
         * @brief Retrieves the value of the designated column as a double.
         * @param columnName The SQL name of the column. case-sensitive.
         * @return The column value; if the value is SQL NULL, the value returned is 0.0.
         * @throws sql_exception If a database access error occurs, columnName does not exist or if the value is NaN.
         */
        [[nodiscard]] double getDouble(const std::string& columnName) {
            except_wrapper(RETURN ResultSet_getDoubleByName(t_, columnName.c_str()));
        }
        
        /**
         * @brief Retrieves the value of the designated column as a byte span.
         * @param columnIndex The first column is 1, the second is 2, ...
         * @return An optional span of bytes containing the blob data, or std::nullopt if the value is SQL NULL.
         * @throws sql_exception If a database access error occurs or columnIndex is outside the valid range.
         */
        [[nodiscard]] std::optional<std::span<const std::byte>> getBlob(int columnIndex) {
            int size = 0;
            const void *blob = nullptr;
            except_wrapper(blob = ResultSet_getBlob(t_, columnIndex, &size));
            if (blob == nullptr || size == 0) {
                return std::nullopt;
            }
            return std::span<const std::byte>(static_cast<const std::byte*>(blob), size);
        }

        /**
         * @brief Retrieves the value of the designated column as a byte span.
         * @param columnName The SQL name of the column. case-sensitive.
         * @return An optional span of bytes containing the blob data, or std::nullopt if the value is SQL NULL.
         * @throws sql_exception If a database access error occurs or columnIndex is outside the valid range.
         */
        [[nodiscard]] std::optional<std::span<const std::byte>> getBlob(const std::string& columnName) {
            int size = 0;
            const void *blob = nullptr;
            except_wrapper(blob = ResultSet_getBlobByName(t_, columnName.c_str(), &size));
            if (blob == nullptr || size == 0) {
                return std::nullopt;
            }
            return std::span<const std::byte>(static_cast<const std::byte*>(blob), size);
        }

        /**
         * @brief Retrieves the value of the designated column as a Unix timestamp.
         * @param columnIndex The first column is 1, the second is 2, ...
         * @return The column value as seconds since the epoch in the GMT timezone.
         *         If the value is SQL NULL, the value returned is 0.
         * @throws sql_exception If a database access error occurs, columnIndex is outside
         *         the valid range or if the column value cannot be converted to a valid timestamp.
         */
        [[nodiscard]] time_t getTimestamp(int columnIndex) {
            except_wrapper(RETURN ResultSet_getTimestamp(t_, columnIndex));
        }
        
        /**
         * @brief Retrieves the value of the designated column as a Unix timestamp.
         * @param columnName The SQL name of the column. case-sensitive.
         * @return The column value as seconds since the epoch in the GMT timezone.
         *         If the value is SQL NULL, the value returned is 0.
         * @throws sql_exception If a database access error occurs, columnName is not found
         *         or if the column value cannot be converted to a valid timestamp.
         */
        [[nodiscard]] time_t getTimestamp(const std::string& columnName) {
            except_wrapper(RETURN ResultSet_getTimestampByName(t_, columnName.c_str()));
        }
        
        /**
         * @brief Retrieves the value of the designated column as a Date, Time or DateTime.
         * @param columnIndex The first column is 1, the second is 2, ...
         * @return A tm structure with fields for date and time.
         *         If the value is SQL NULL, a zeroed tm structure is returned. Use isNull() if in doubt.
         * @throws sql_exception If a database access error occurs, columnIndex is outside the valid range
         *         or if the column value cannot be converted to a valid SQL Date, Time or DateTime type.
         */
        [[nodiscard]] tm getDateTime(int columnIndex) {
            except_wrapper(RETURN ResultSet_getDateTime(t_, columnIndex));
        }
        
        /**
         * @brief Retrieves the value of the designated column as a Date, Time or DateTime.
         * @param columnName The SQL name of the column. case-sensitive.
         * @return A tm structure with fields for date and time.
         *         If the value is SQL NULL, a zeroed tm structure is returned. Use isNull() if in doubt.
         * @throws sql_exception If a database access error occurs, columnName is not found
         *         or if the column value cannot be converted to a valid SQL Date, Time or DateTime type.
         */
        [[nodiscard]] tm getDateTime(const std::string& columnName) {
            except_wrapper(RETURN ResultSet_getDateTimeByName(t_, columnName.c_str()));
        }
        
    protected:
        friend class PreparedStatement;
        friend class Connection;
        
        /**
         * @brief Constructs a ResultSet from a ResultSet_T.
         * @param t The underlying ResultSet_T.
         */
        explicit ResultSet(ResultSet_T t) : t_(t) {}
        
    private:
        ResultSet_T t_;
    };

    
    /**
     * @class PreparedStatement
     * @brief Represents a single SQL statement pre-compiled into byte code.
     *
     * A PreparedStatement can contain SQL parameters, which are set using the
     * various set methods. A PreparedStatement is created by calling
     * Connection::prepareStatement().
     *
     * Example usage:
     * @code
     * PreparedStatement stmt = con.prepareStatement("INSERT INTO employee(name, picture) VALUES(?, ?)");
     * stmt.setString(1, "Kamiya Kaoru");
     * stmt.setBlob(2, jpeg);
     * stmt.execute();
     * @endcode
     */
    class PreparedStatement : private noncopyable {
    public:
        /**
         * @brief Move constructor.
         * @param r PreparedStatement object to move.
         */
        PreparedStatement(PreparedStatement&& r) noexcept : t_(r.t_) { r.t_ = nullptr; }
        
        /**
         * @brief Conversion operator to PreparedStatement_T.
         * @return The underlying PreparedStatement_T.
         */
        operator PreparedStatement_T() const noexcept { return t_; }
        
        /**
         * @brief Sets the string parameter at the given index.
         * @param parameterIndex The first parameter is 1, the second is 2,..
         * @param x The string value to set.
         * @throws sql_exception If a database access error occurs or parameterIndex
         *         is outside the valid range
         * @note The string data must remain valid until execute() or executeQuery()
         *       is called. This method does not copy the string data.
         */
        void setString(int parameterIndex, std::string_view x) {
            storage_[parameterIndex] = x;
            except_wrapper(PreparedStatement_setString(t_, parameterIndex, x.data()));
        }
        
        /**
         * @brief Sets the string parameter at the given index.
         * @param parameterIndex The first parameter is 1, the second is 2,..
         * @param x The string value to set.
         * @throws sql_exception If a database access error occurs or parameterIndex
         *         is outside the valid range
         * @note The string data must remain valid until execute() or executeQuery()
         *       is called. This method does not copy the string data.
         */
        void setString(int parameterIndex, const std::string& x) {
            setString(parameterIndex, std::string_view(x));
        }
        
        /**
         * @brief Sets the integer parameter at the given index.
         * @param parameterIndex The first parameter is 1, the second is 2,..
         * @param x The integer value to set.
         * @throws sql_exception If a database access error occurs or parameterIndex
         *         is outside the valid range
         */
        void setInt(int parameterIndex, int x) {
            except_wrapper(PreparedStatement_setInt(t_, parameterIndex, x));
        }
        
        /**
         * @brief Sets the long long parameter at the given index.
         * @param parameterIndex The first parameter is 1, the second is 2,..
         * @param x The long long value to set.
         * @throws sql_exception If a database access error occurs or parameterIndex
         *         is outside the valid range
         */
        void setLLong(int parameterIndex, long long x) {
            except_wrapper(PreparedStatement_setLLong(t_, parameterIndex, x));
        }
        
        /**
         * @brief Sets the double parameter at the given index.
         * @param parameterIndex The first parameter is 1, the second is 2,..
         * @param x The double value to set.
         * @throws sql_exception If a database access error occurs or parameterIndex
         *         is outside the valid range
         */
        void setDouble(int parameterIndex, double x) {
            except_wrapper(PreparedStatement_setDouble(t_, parameterIndex, x));
        }
        
        /**
         * @brief Sets the blob parameter at the given index.
         * @param parameterIndex The first parameter is 1, the second is 2,..
         * @param x The blob value to set.
         * @throws sql_exception If a database access error occurs or parameterIndex
         *         is outside the valid range
         * @note The blob data must remain valid until execute() or executeQuery()
         *       is called. This method does not copy the blob data.
         */
        void setBlob(int parameterIndex, std::span<const std::byte> x) {
            if (x.empty()) {
                setNull(parameterIndex);
            } else {
                storage_[parameterIndex] = x;
                except_wrapper(PreparedStatement_setBlob(t_, parameterIndex, x.data(), static_cast<int>(x.size())));
            }
        }
        
        /**
         * @brief Sets the timestamp parameter at the given index.
         * @param parameterIndex The first parameter is 1, the second is 2,..
         * @param x The timestamp value to set.
         * @throws sql_exception If a database access error occurs or parameterIndex
         *         is outside the valid range
         */
        void setTimestamp(int parameterIndex, time_t x) {
            except_wrapper(PreparedStatement_setTimestamp(t_, parameterIndex, x));
        }
        
        /**
         * @brief Sets the parameter at the given index to SQL NULL.
         * @param parameterIndex The first parameter is 1, the second is 2,..
         * @throws sql_exception If a database access error occurs or parameterIndex
         *         is outside the valid range
         */
        void setNull(int parameterIndex) {
            except_wrapper(PreparedStatement_setNull(t_, parameterIndex));
        }
        
        /**
         * @brief Executes the prepared SQL statement.
         * @throws sql_exception If a database access error occurs or parameterIndex
         *         is outside the valid range
         */
        void execute() {
            except_wrapper(PreparedStatement_execute(t_));
            storage_.clear();
        }
        
        /**
         * @brief Executes the prepared SQL query.
         * @return A ResultSet containing the query results.
         * @throws sql_exception If a database access error occurs or parameterIndex
         *         is outside the valid range
         */
        [[nodiscard]] ResultSet executeQuery() {
            except_wrapper(
                           ResultSet_T r = PreparedStatement_executeQuery(t_);
                           storage_.clear();
                           RETURN ResultSet(r);
                           );
        }
        
        /**
         * @brief Gets the number of rows changed by the last statement.
         * @return The number of rows changed.
         */
        [[nodiscard]] long long rowsChanged() noexcept {
            return PreparedStatement_rowsChanged(t_);
        }
        
        /**
         * @brief Gets the number of parameters in the prepared statement.
         * @return The number of parameters.
         */
        [[nodiscard]] int getParameterCount() noexcept {
            return PreparedStatement_getParameterCount(t_);
        }
        
    public:
        /**
         * @brief Binds a string value to the prepared statement.
         * @param parameterIndex The first parameter is 1, the second is 2,..
         * @param x The string value to bind.
         * @throws sql_exception If a database access error occurs or parameterIndex
         *         is outside the valid range
         * @note The string data must remain valid until execute() or executeQuery()
         *       is called. This method does not copy the string data.
         */
        void bind(int parameterIndex, const std::string& x) {
            setString(parameterIndex, x);
        }
        
        /**
         * @brief Binds a string value to the prepared statement.
         * @param parameterIndex The first parameter is 1, the second is 2,..
         * @param x The string value to bind.
         * @throws sql_exception If a database access error occurs or parameterIndex
         *         is outside the valid range
         * @note The string data must remain valid until execute() or executeQuery()
         *       is called. This method does not copy the string data.
         */
        void bind(int parameterIndex, std::string_view x) {
            setString(parameterIndex, x);
        }
        
        /**
         * @brief Binds a string value to the prepared statement.
         * @param parameterIndex The first parameter is 1, the second is 2,..
         * @param x The string value to bind.
         * @throws sql_exception If a database access error occurs or parameterIndex
         *         is outside the valid range
         * @note The string data must remain valid until execute() or executeQuery()
         *       is called. This method does not copy the string data.
         */
        void bind(int parameterIndex, const char* x) {
            if (x == nullptr) {
                setNull(parameterIndex);
            } else {
                setString(parameterIndex, static_cast<std::string_view>(x));
            }
        }
        
        /**
         * @brief Binds an arithmetic type (int, double, etc.) and time_t to the prepared statement.
         * @param parameterIndex The first parameter is 1, the second is 2,..
         * @param x The value to bind.
         * @throws sql_exception If a database access error occurs or parameterIndex
         *         is outside the valid range
         */
        template<typename T>
        typename std::enable_if_t<
        std::is_arithmetic_v<T> || std::is_same_v<T, time_t>,
        void
        > bind(int parameterIndex, T x) {
            if constexpr (std::is_same_v<T, time_t>) {
                setTimestamp(parameterIndex, x);
            } else if constexpr (std::is_integral_v<T>) {
                if constexpr (sizeof(T) <= sizeof(int)) {
                    setInt(parameterIndex, static_cast<int>(x));
                } else {
                    setLLong(parameterIndex, static_cast<long long>(x));
                }
            } else if constexpr (std::is_floating_point_v<T>) {
                setDouble(parameterIndex, static_cast<double>(x));
            }
        }
        
        /**
         * @brief Binds the parameter at the given index to SQL NULL.
         * @param parameterIndex The first parameter is 1, the second is 2,..
         * @throws sql_exception If a database access error occurs or parameterIndex
         *         is outside the valid range
         */
        void bind(int parameterIndex, std::nullptr_t) {
            setNull(parameterIndex);
        }
        
        /**
         * @brief Binds blob data to the prepared statement.
         * @param parameterIndex The first parameter is 1, the second is 2,..
         * @param x The blob value to set.
         * @throws sql_exception If a database access error occurs or parameterIndex
         *         is outside the valid range
         * @note The blob data must remain valid until execute() or executeQuery()
         *       is called. This method does not copy the blob data.
         */
        void bind(int parameterIndex, std::span<const std::byte> x) {
            setBlob(parameterIndex, x);
        }
        
    protected:
        friend class Connection;
        
        /**
         * @brief Constructs a PreparedStatement from a PreparedStatement_T.
         * @param t The underlying PreparedStatement_T.
         */
        explicit PreparedStatement(PreparedStatement_T t) : t_(t) {}
        
    private:
        PreparedStatement_T t_;
        // A store to ensure that we have valid references to the data
        std::unordered_map<int, std::variant<std::string_view, std::span<const std::byte>>> storage_;
    };

    
    /**
     * @class Connection
     * @brief A `Connection` represent a connection to a SQL database system.
     *
     * The Connection class is used to execute SQL statements, manage transactions,
     * and retrieve results. It supports executing statements directly or using
     * prepared statements.
     *
     * A Connection instance should only be used by one thread at a time.
     */
    class Connection : private noncopyable {
    public:
        /**
         * @brief Destructor, closes the connection and return the underlying connection to the connection pool
         */
        ~Connection() { if (t_) close(); }
        
        /**
         * @brief Implicit conversion to the underlying Connection_T type.
         * @return The underlying Connection_T object.
         */
        operator Connection_T() const noexcept { return t_; }
        
        /**
         * @brief Sets the query timeout.
         * @param ms Timeout in milliseconds.
         */
        void setQueryTimeout(int ms) noexcept { Connection_setQueryTimeout(t_, ms); }
        
        /**
         * @brief Gets the current query timeout.
         * @return The query timeout in milliseconds.
         */
        [[nodiscard]] int getQueryTimeout() noexcept { return Connection_getQueryTimeout(t_); }
        
        /**
         * @brief Sets the maximum number of rows a ResultSet can contain.
         * @param max Maximum number of rows.
         */
        void setMaxRows(int max) noexcept { Connection_setMaxRows(t_, max); }
        
        /**
         * @brief Gets the maximum number of rows a ResultSet can contain.
         * @return The maximum number of rows.
         */
        [[nodiscard]] int getMaxRows() noexcept { return Connection_getMaxRows(t_); }
        
        /**
         * @brief Sets the number of rows to fetch when more rows are needed.
         * @param rows Number of rows to fetch.
         */
        void setFetchSize(int rows) noexcept { Connection_setFetchSize(t_, rows); }
        
        /**
         * @brief Gets the number of rows to fetch when more rows are needed.
         * @return The number of rows to fetch.
         */
        [[nodiscard]] int getFetchSize() noexcept { return Connection_getFetchSize(t_); }
        
        /**
         * @brief Pings the database server to check if the connection is alive.
         * @return True if the connection is alive, false otherwise.
         */
        [[nodiscard]] bool ping() noexcept { return Connection_ping(t_); }
        
        /**
         * @brief Clears any pending results or statements.
         */
        void clear() noexcept { Connection_clear(t_); }
        
        /**
         * @brief Closes the connection and return the underlying connection to the connection pool
         */
        void close() noexcept {
            if (t_) {
                Connection_close(t_);
                setClosed();
            }
        }
        
        /**
         * @brief Begins a new transaction.
         * @throws SQLException If a database error occurs.
         */
        void beginTransaction() { except_wrapper(Connection_beginTransaction(t_)); }
        
        /**
         * @brief Commits the current transaction.
         * @throws SQLException If a database error occurs.
         */
        void commit() { except_wrapper(Connection_commit(t_)); }
        
        /**
         * @brief Rolls back the current transaction.
         * @throws SQLException If a database error occurs.
         */
        void rollback() { except_wrapper(Connection_rollback(t_)); }
        
        /**
         * @brief Gets the last inserted row ID.
         * @return The last inserted row ID.
         */
        [[nodiscard]] long long lastRowId() noexcept { return Connection_lastRowId(t_); }
        
        /**
         * @brief Gets the number of rows changed by the last operation.
         * @return The number of rows changed.
         */
        [[nodiscard]] long long rowsChanged() noexcept { return Connection_rowsChanged(t_); }
        
        /**
         * @brief Executes a SQL statement.
         * @param sql The SQL statement to execute.
         * @throws SQLException If a database error occurs.
         */
        void execute(const std::string& sql) { except_wrapper(Connection_execute(t_, "%s", sql.c_str())); }
        
        /**
         * @brief Executes a parameterized SQL statement.
         * @param sql The SQL statement with placeholders.
         * @param args The arguments to bind to the placeholders.
         * @throws SQLException If a database error occurs.
         */
        template <typename ...Args>
        void execute(const std::string& sql, Args&&... args) {
            PreparedStatement p(this->prepareStatement(sql));
            int i = 1;
            (p.bind(i++, std::forward<Args>(args)), ...);
            p.execute();
        }
        
        /**
         * @brief Executes a SQL query and returns a ResultSet.
         * @param sql The SQL query to execute.
         * @return A ResultSet containing the query results.
         * @throws SQLException If a database error occurs.
         */
        [[nodiscard]] ResultSet executeQuery(const std::string& sql) {
            except_wrapper(
                           ResultSet_T r = Connection_executeQuery(t_, "%s", sql.c_str());
                           RETURN ResultSet(r);
                           );
        }
        
        /**
         * @brief Executes a parameterized SQL query and returns a ResultSet.
         * @param sql The SQL query with placeholders.
         * @param args The arguments to bind to the placeholders.
         * @return A ResultSet containing the query results.
         * @throws SQLException If a database error occurs.
         */
        template <typename ...Args>
        [[nodiscard]] ResultSet executeQuery(const std::string& sql, Args&&... args) {
            PreparedStatement p(this->prepareStatement(sql));
            int i = 1;
            (p.bind(i++, std::forward<Args>(args)), ...);
            return p.executeQuery();
        }
        
        /**
         * @brief Prepares a SQL statement for execution.
         * @param sql The SQL statement to prepare.
         * @return A PreparedStatement object.
         * @throws SQLException If a database error occurs.
         */
        [[nodiscard]] PreparedStatement prepareStatement(const std::string& sql) {
            except_wrapper(
                           PreparedStatement_T p = Connection_prepareStatement(t_, "%s", sql.c_str());
                           RETURN PreparedStatement(p);
                           );
        }
        
        /**
         * @brief Prepares a parameterized SQL statement for execution.
         * @param sql The SQL statement with placeholders.
         * @param args The arguments to bind to the placeholders.
         * @return A PreparedStatement object.
         * @throws SQLException If a database error occurs.
         */
        template <typename ...Args>
        [[nodiscard]] PreparedStatement prepareStatement(const std::string& sql, Args&&... args) {
            except_wrapper(
                           PreparedStatement p(this->prepareStatement(sql));
                           int i = 1;
                           (p.bind(i++, std::forward<Args>(args)), ...);
                           RETURN p;
                           );
        }
        
        /**
         * @brief Gets the last error message.
         * @return The last error message as a string view.
         */
        [[nodiscard]] const std::string_view getLastError() const noexcept {
            return Connection_getLastError(t_);
        }
        
        /**
         * @brief Checks if the specified database system is supported.
         * @param url The database URL or protocol.
         * @return True if supported, false otherwise.
         */
        [[nodiscard]] static bool isSupported(const std::string& url) noexcept {
            return Connection_isSupported(url.c_str());
        }
        
    protected:
        friend class ConnectionPool;
        
        /**
         * @brief Constructs a Connection with an existing Connection_T object.
         * @param C The existing Connection_T object.
         */
        explicit Connection(Connection_T C) : t_(C) {}
        
        /**
         * @brief Marks the connection as closed.
         */
        void setClosed() noexcept { t_ = nullptr; }
        
    private:
        Connection_T t_;
    };

    
    /**
     * @class ConnectionPool
     * @brief A `ConnectionPool` represent a database connection pool
     *
     * The ConnectionPool class handles creating, managing, and providing
     * connections to a database. It supports setting various properties
     * like initial and maximum connections, connection timeouts, and more.
     * It also includes methods for starting and stopping the pool, as well
     * as obtaining and returning connections.
     *
     * This ConnectionPool is thread-safe.
     */
    class ConnectionPool : private noncopyable {
    public:
        /**
         * @brief Constructs a ConnectionPool with the given URL string.
         * @param url The database connection URL string.
         * @throws sql_exception If the URL is invalid.
         */
        explicit ConnectionPool(const std::string& url) : url_(url) {
            if (!url_)
                throw sql_exception("Invalid URL");
            t_ = ConnectionPool_new(url_);
        }
        
        /**
         * @brief Constructs a ConnectionPool by taking ownership of the given URL object.
         * @param url The database connection URL object to move from.
         * @throws sql_exception If the URL is invalid.
         * @note After this operation, the passed URL object is left in a valid but unspecified state.
         *       It's safe to destroy or reassign the moved-from URL object, but using its value is undefined.
         */
        explicit ConnectionPool(URL&& url) : url_(std::move(url)) {
            if (!url_)
                throw sql_exception("Invalid URL");
            t_ = ConnectionPool_new(url_);
        }
        
        /**
         * @brief Destructor, releases resources allocated by the pool.
         */
        ~ConnectionPool() { ConnectionPool_free(&t_); }
        
        /**
         * @brief Implicit conversion to the underlying ConnectionPool_T type.
         * @return The underlying ConnectionPool_T object.
         */
        operator ConnectionPool_T() noexcept { return t_; }
        
        /**
         * @brief Gets the URL of the connection pool.
         * @return The URL of the connection pool.
         */
        [[nodiscard]] const URL& getURL() const noexcept { return url_; }
        
        /**
         * @brief Sets the number of initial connections in the pool.
         * @param connections The number of initial connections.
         */
        void setInitialConnections(int connections) noexcept {
            ConnectionPool_setInitialConnections(t_, connections);
        }
        
        /**
         * @brief Gets the number of initial connections in the pool.
         * @return The number of initial connections.
         */
        [[nodiscard]] int getInitialConnections() noexcept {
            return ConnectionPool_getInitialConnections(t_);
        }
        
        /**
         * @brief Sets the maximum number of connections in the pool.
         * @param maxConnections The maximum number of connections.
         */
        void setMaxConnections(int maxConnections) noexcept {
            ConnectionPool_setMaxConnections(t_, maxConnections);
        }
        
        /**
         * @brief Gets the maximum number of connections in the pool.
         * @return The maximum number of connections.
         */
        [[nodiscard]] int getMaxConnections() noexcept {
            return ConnectionPool_getMaxConnections(t_);
        }
        
        /**
         * @brief Sets the connection timeout value.
         * @param connectionTimeout The timeout value in seconds.
         */
        void setConnectionTimeout(int connectionTimeout) noexcept {
            ConnectionPool_setConnectionTimeout(t_, connectionTimeout);
        }
        
        /**
         * @brief Gets the connection timeout value.
         * @return The connection timeout value in seconds.
         */
        [[nodiscard]] int getConnectionTimeout() noexcept {
            return ConnectionPool_getConnectionTimeout(t_);
        }
        
        using AbortHandler = std::function<void(std::string_view)>;

        /**
         * @brief Set the function to call if a fatal error occurs in the library
         * In practice this means Out-Of-Memory errors or uncatched exceptions.
         * Clients may optionally provide this function. If not provided
         * the library will call `abort(3)` or `exit(1)` upon encountering a fatal error
         * It is an unchecked runtime error to continue using the library after
         * the `abortHandler` was called.
         * @param abortHandler The handler function to call on fatal errors.
         */
        void setAbortHandler(AbortHandler abortHandler = nullptr) noexcept {
            g_abortHandler = std::move(abortHandler);
            ConnectionPool_setAbortHandler(t_, g_abortHandler ? bridgeAbortHandler : nullptr);
        }

        /**
         * @brief Sets the reaper thread sweep interval.
         * @param sweepInterval The sweep interval in seconds.
         */
        void setReaper(int sweepInterval) noexcept {
            ConnectionPool_setReaper(t_, sweepInterval);
        }
        
        /**
         * @brief Gets the current number of connections in the pool.
         * @return The total number of connections in the pool.
         */
        [[nodiscard]] int size() noexcept { return ConnectionPool_size(t_); }
        
        /**
         * @brief Gets the number of active connections in the pool.
         * @return The number of active connections in the pool.
         */
        [[nodiscard]] int active() noexcept { return ConnectionPool_active(t_); }
        
        /**
         * @brief Starts the connection pool.
         * @throws SQLException If a database error occurs.
         */
        void start() { except_wrapper(ConnectionPool_start(t_)); }
        
        /**
         * @brief Stops the connection pool.
         * @throws sql_exception If there are active connections.
         */
        void stop() {
            if (this->active() > 0) {
                throw sql_exception(
                                    "Trying to stop the pool with active Connections. "
                                    "Please close all active Connections first");
            }
            ConnectionPool_stop(t_);
        }
        
        /**
         * @brief Obtains a connection from the pool.
         * @return A Connection object.
         * @throws SQLException If a database error occurs.
         */
        [[nodiscard]] Connection getConnection() {
            except_wrapper(
                           Connection_T c = ConnectionPool_getConnectionOrException(t_);
                           RETURN Connection(c);
                           );
        }
        
        /**
         * @brief Returns a connection to the pool.
         * @param con The Connection to return.
         */
        void returnConnection(Connection& con) noexcept { con.close(); }
        
        /**
         * @brief Reaps inactive connections in the pool.
         * @return The number of connections reaped.
         */
        int reapConnections() noexcept {
            return ConnectionPool_reapConnections(t_);
        }
        
        /**
         * @brief Gets the library version information.
         * @return The version information as a string view.
         */
        [[nodiscard]] static std::string_view version() noexcept {
            return ConnectionPool_version();
        }
        
    private:
        URL url_;
        ConnectionPool_T t_;
    };

} // namespace zdb

#endif

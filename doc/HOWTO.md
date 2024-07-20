 ## Configuring the Connection Pool using the URL:
 
 You can add custom parameters to the URL to configure various aspects of the
 pool. For example, you might have:
 
 ```c
 const char *db_url = getConfigValue("DATABASE_URL");  // Your config reading function
 URL_T url = URL_new(db_url);
 ```
 
 This URL might have custom parameters for configuring the pool, for instance,
 initial and max connections and look like this:
 
 `mysql://root:swordfish@localhost/test?initialConnections=10&maxConnections=50` 
 
 Here's an example of how you would use these URL parameters to configure your
 pool:
 
 ```c
 ConnectionPool_T pool = ConnectionPool_new(url);
 // Read custom parameters of the URL and use them to configure the pool before
 // starting:
 if (URL_getParameter(url, "initialConnections")) {
     ConnectionPool_setInitialConnections(pool, 
         Str_parseInt(URL_getParameter(url, "initialConnections")));
 }
 if (URL_getParameter(url, "maxConnections")) {
     ConnectionPool_setMaxConnections(pool, 
         Str_parseInt(URL_getParameter(url, "maxConnections")));
 }
 ConnectionPool_start(pool);
 ```
 
 Note: The parameter names used here (like "initialConnections" and
 "maxConnections") are just examples. You can choose your own parameter names to
 match your application's conventions. Additionally, you can add other
 parameters as needed for your specific use case, following the same pattern
 shown above.
 
 This configuration method provides a flexible and centralized way to set up
 your connection pool, making it easier to manage different configurations for
 various environments or use cases, and adapt to your specific application needs.

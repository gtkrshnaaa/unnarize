#include "ucore_http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

// URL decode helper
static void urlDecode(char* dst, const char* src) {
    char a, b;
    while (*src) {
        if ((*src == '%') && ((a = src[1]) && (b = src[2])) && isxdigit(a) && isxdigit(b)) {
            if (a >= 'a') a -= 'a'-'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a'-'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = 16*a + b;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

// Parse query string into Map: "foo=bar&baz=qux" -> {foo: "bar", baz: "qux"}
static void parseQueryString(VM* vm, Map* queryMap, const char* queryStr) {
    if (!queryStr || !*queryStr) return;
    
    char* copy = strdup(queryStr);
    char* token = strtok(copy, "&");
    while (token) {
        char* eq = strchr(token, '=');
        if (eq) {
            *eq = '\0';
            char key[256], val[1024];
            urlDecode(key, token);
            urlDecode(val, eq + 1);
            
            ObjString* valStr = internString(vm, val, (int)strlen(val));
            mapSetStr(queryMap, key, (int)strlen(key), OBJ_VAL(valStr));
        }
        token = strtok(NULL, "&");
    }
    free(copy);
}

// Parse headers into Map
static void parseHeaders(VM* vm, Map* headerMap, const char* request) {
    const char* line = strchr(request, '\n');
    if (!line) return;
    line++; // Skip first line (GET /path HTTP/1.1)
    
    while (line && *line && *line != '\r' && *line != '\n') {
        const char* colon = strchr(line, ':');
        const char* end = strchr(line, '\r');
        if (!end) end = strchr(line, '\n');
        if (!colon || !end) break;
        
        // Extract key and value
        int keyLen = colon - line;
        const char* valStart = colon + 1;
        while (*valStart == ' ') valStart++;
        int valLen = end - valStart;
        
        if (keyLen > 0 && keyLen < 256 && valLen >= 0 && valLen < 1024) {
            char key[256];
            memcpy(key, line, keyLen);
            key[keyLen] = '\0';
            
            // Convert key to lowercase for consistency
            for (int i = 0; i < keyLen; i++) key[i] = tolower(key[i]);
            
            ObjString* valStr = internString(vm, valStart, valLen);
            mapSetStr(headerMap, key, keyLen, OBJ_VAL(valStr));
        }
        
        line = end + 1;
        if (*line == '\n') line++;
    }
}

// Helper to parse URL check for http://
static bool parseUrl(const char* url, char* hostOut, char* pathOut, int* portOut) {
    // Simple parser: http://host:port/path
    const char* p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    else if (strncmp(p, "https://", 8) == 0) {
        printf("Error: HTTPS not supported in uCoreHttp (no OpenSSL).\n");
        return false;
    }
    
    // Parse host
    const char* start = p;
    while (*p && *p != ':' && *p != '/') p++;
    int hostLen = p - start;
    if (hostLen >= 256) return false;
    strncpy(hostOut, start, hostLen);
    hostOut[hostLen] = '\0';
    
    // Port
    *portOut = 80;
    if (*p == ':') {
        p++;
        *portOut = atoi(p);
        while (*p && *p != '/') p++;
    }
    
    // Path
    if (*p == '/') {
        // strncpy triggers warning for truncation, use safer copy
        size_t len = strlen(p);
        if (len >= 1024) len = 1023;
        memcpy(pathOut, p, len);
        pathOut[len] = '\0';
    } else {
        strcpy(pathOut, "/");
    }
    return true;
}


// ---- JSON Helper ----
static void json_append(char** buf, int* len, int* cap, const char* str) {
    int l = (int)strlen(str);
    if (*len + l >= *cap) {
        *cap = *cap * 2 + l + 1024;
        *buf = realloc(*buf, *cap);
    }
    strcpy(*buf + *len, str);
    *len += l;
}

static void json_serialize_val(Value v, char** buf, int* len, int* cap) {
    char temp[64];
    if (IS_NIL(v)) {
        json_append(buf, len, cap, "null");
        return;
    }
    
    switch (getValueType(v)) {
        case VAL_INT:
            snprintf(temp, sizeof(temp), "%lld", (long long)AS_INT(v));
            json_append(buf, len, cap, temp);
            break;
        case VAL_FLOAT:
            snprintf(temp, sizeof(temp), "%.6g", AS_FLOAT(v));
            json_append(buf, len, cap, temp);
            break;
        case VAL_BOOL:
            json_append(buf, len, cap, AS_BOOL(v) ? "true" : "false");
            break;
        case VAL_OBJ: {
            Obj* o = AS_OBJ(v);
            if (o->type == OBJ_STRING) {
                json_append(buf, len, cap, "\"");
                json_append(buf, len, cap, ((ObjString*)o)->chars);
                json_append(buf, len, cap, "\"");
            } else if (o->type == OBJ_ARRAY) {
                Array* a = (Array*)o;
                json_append(buf, len, cap, "[");
                for (int i = 0; i < a->count; i++) {
                    if (i > 0) json_append(buf, len, cap, ",");
                    json_serialize_val(a->items[i], buf, len, cap);
                }
                json_append(buf, len, cap, "]");
            } else if (o->type == OBJ_MAP) {
                Map* m = (Map*)o;
                json_append(buf, len, cap, "{");
                bool first = true;
                for (int i = 0; i < TABLE_SIZE; i++) {
                    MapEntry* e = m->buckets[i];
                    while (e) {
                         if (!first) json_append(buf, len, cap, ",");
                         first = false;
                         json_append(buf, len, cap, "\"");
                         json_append(buf, len, cap, e->key); // Assume keys are strings
                         json_append(buf, len, cap, "\":");
                         json_serialize_val(e->value, buf, len, cap);
                         e = e->next;
                    }
                }
                json_append(buf, len, cap, "}");
            } else {
                json_append(buf, len, cap, "\"<obj>\"");
            }
            break;
        }
        default:
            json_append(buf, len, cap, "null");
            break;
    }
}

// http.json(data) -> string
static Value uhttp_json(VM* vm, Value* args, int argCount) {
    if (argCount != 1) {
        ObjString* empty = internString(vm, "{}", 2);
        Value v = OBJ_VAL(empty);
        return v;
    }
    
    int cap = 1024;
    int len = 0;
    char* buf = malloc(cap);
    buf[0] = '\0';
    
    json_serialize_val(args[0], &buf, &len, &cap);
    
    ObjString* s = internString(vm, buf, len);
    free(buf);
    
    Value v = OBJ_VAL(s);
    return v;
}

static Value http_perform(VM* vm, const char* method, const char* url, const char* body) {
    char host[256];
    char path[1024];
    int port;
    
    if (!parseUrl(url, host, path, &port)) {
        printf("Invalid URL or HTTPS not supported.\n");
        return NIL_VAL; 
    }
    
    // DNS Resolve
    struct hostent* he = gethostbyname(host);
    if (!he) {
        printf("Could not resolve host: %s\n", host);
        return NIL_VAL; // null
    }
    
    // Socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return NIL_VAL;
    }
    
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    // Modern way (h_addr_list)
    memcpy(&serv_addr.sin_addr, he->h_addr_list[0], he->h_length);
    
    // Connect
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        close(sockfd);
        return NIL_VAL;
    }
    
    // Construct Request
    char req[4096];
    int contentLen = body ? (int)strlen(body) : 0;
    snprintf(req, sizeof(req), 
        "%s %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: Unnarize/1.0\r\n"
        "Connection: close\r\n"
        "Content-Length: %d\r\n"
        "\r\n" // End headers
        "%s", 
        method, path, host, contentLen, body ? body : "");
        
    // Send
    if (send(sockfd, req, strlen(req), 0) < 0) {
        perror("Send failed");
        close(sockfd);
        return NIL_VAL;
    }
    
    // Receive
    int cap = 4096;
    int size = 0;
    char* buf = malloc(cap);
    
    while (1) {
        if (size + 1024 >= cap) {
            cap *= 2;
            buf = realloc(buf, cap);
        }
        int n = recv(sockfd, buf + size, cap - size - 1, 0);
        if (n <= 0) break; // closed or error
        size += n;
    }
    buf[size] = '\0';
    close(sockfd);
    
    // Separate body
    char* bodyStart = strstr(buf, "\r\n\r\n");
    if (bodyStart) {
        bodyStart += 4;
    } else {
        bodyStart = buf;
    }
    
    ObjString* os = internString(vm, bodyStart, (int)strlen(bodyStart));
    free(buf);
    
    Value vRes = OBJ_VAL(os);
    return vRes;
}

static Value uhttp_get(VM* vm, Value* args, int argCount) {
    if (argCount != 1 || !IS_STRING(args[0])) {
        printf("Error: ucoreHttp.get(url) expects url string.\n");
        return NIL_VAL;
    }
    return http_perform(vm, "GET", AS_CSTRING(args[0]), NULL);
}

static Value uhttp_post(VM* vm, Value* args, int argCount) {
    if (argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        printf("Error: ucoreHttp.post(url, body) expects url and body strings.\n");
        return NIL_VAL;
    }
    return http_perform(vm, "POST", AS_CSTRING(args[0]), AS_CSTRING(args[1]));
}

static Value uhttp_put(VM* vm, Value* args, int argCount) {
    if (argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        printf("Error: ucoreHttp.put(url, body) expects url and body strings.\n");
        return NIL_VAL;
    }
    return http_perform(vm, "PUT", AS_CSTRING(args[0]), AS_CSTRING(args[1]));
}

static Value uhttp_delete(VM* vm, Value* args, int argCount) {
    if (argCount != 1 || !IS_STRING(args[0])) {
        printf("Error: ucoreHttp.delete(url) expects url string.\n");
        return NIL_VAL;
    }
    return http_perform(vm, "DELETE", AS_CSTRING(args[0]), NULL);
}

static Value uhttp_patch(VM* vm, Value* args, int argCount) {
    if (argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        printf("Error: ucoreHttp.patch(url, body) expects url and body strings.\n");
        return NIL_VAL;
    }
    return http_perform(vm, "PATCH", AS_CSTRING(args[0]), AS_CSTRING(args[1]));
}


// ---- Routing ----
typedef struct Route {
    char* method;
    char* path;
    char* handlerName;
    struct Route* next;
} Route;

static Route* g_routes = NULL;

// ---- Middleware ----
typedef struct Middleware {
    char* handlerName;
    struct Middleware* next;
} Middleware;

static Middleware* g_middleware = NULL;

// ---- Static Files ----
typedef struct StaticMount {
    char* urlPrefix;
    char* dirPath;
    struct StaticMount* next;
} StaticMount;

static StaticMount* g_static = NULL;

// Add middleware
static Value uhttp_use(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount != 1 || !IS_STRING(args[0])) {
        printf("Error: ucoreHttp.use(handlerName) expects string.\n");
        return BOOL_VAL(false);
    }
    
    Middleware* m = malloc(sizeof(Middleware));
    m->handlerName = strdup(AS_CSTRING(args[0]));
    m->next = g_middleware;
    g_middleware = m;
    
    return BOOL_VAL(true);
}

// Mount static files
static Value uhttp_static(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        printf("Error: ucoreHttp.static(urlPrefix, dirPath) expects 2 strings.\n");
        return BOOL_VAL(false);
    }
    
    StaticMount* s = malloc(sizeof(StaticMount));
    s->urlPrefix = strdup(AS_CSTRING(args[0]));
    s->dirPath = strdup(AS_CSTRING(args[1]));
    s->next = g_static;
    g_static = s;
    
    return BOOL_VAL(true);
}

// Try to serve static file, returns true if served
static bool tryServeStatic(int socket, const char* path) {
    StaticMount* s = g_static;
    while (s) {
        size_t prefixLen = strlen(s->urlPrefix);
        if (strncmp(path, s->urlPrefix, prefixLen) == 0) {
            // Build file path
            char filePath[2048];
            snprintf(filePath, sizeof(filePath), "%s%s", s->dirPath, path + prefixLen);
            
            FILE* f = fopen(filePath, "rb");
            if (f) {
                // Get file size
                fseek(f, 0, SEEK_END);
                long size = ftell(f);
                fseek(f, 0, SEEK_SET);
                
                // Determine content type
                const char* ct = "application/octet-stream";
                if (strstr(filePath, ".html")) ct = "text/html";
                else if (strstr(filePath, ".css")) ct = "text/css";
                else if (strstr(filePath, ".js")) ct = "application/javascript";
                else if (strstr(filePath, ".json")) ct = "application/json";
                else if (strstr(filePath, ".png")) ct = "image/png";
                else if (strstr(filePath, ".jpg") || strstr(filePath, ".jpeg")) ct = "image/jpeg";
                else if (strstr(filePath, ".svg")) ct = "image/svg+xml";
                
                // Send headers
                char header[512];
                snprintf(header, sizeof(header),
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: %s\r\n"
                    "Content-Length: %ld\r\n"
                    "\r\n", ct, size);
                send(socket, header, strlen(header), 0);
                
                // Send file content
                char buf[4096];
                size_t n;
                while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
                    send(socket, buf, n, 0);
                }
                fclose(f);
                return true;
            }
        }
        s = s->next;
    }
    return false;
}

// Match route pattern with URL params (e.g., /users/:id matches /users/123)
// Returns true if matched, and populates paramsMap with extracted params
static bool matchRoute(const char* pattern, const char* path, VM* vm, Map* paramsMap) {
    const char* p = pattern;
    const char* u = path;
    
    while (*p && *u) {
        if (*p == ':') {
            // Extract param name
            p++; // Skip ':'
            const char* nameStart = p;
            while (*p && *p != '/') p++;
            int nameLen = p - nameStart;
            
            // Extract param value from URL
            const char* valStart = u;
            while (*u && *u != '/') u++;
            int valLen = u - valStart;
            
            // Add to params map
            char name[256];
            if (nameLen < 256) {
                memcpy(name, nameStart, nameLen);
                name[nameLen] = '\0';
                
                ObjString* valStr = internString(vm, valStart, valLen);
                mapSetStr(paramsMap, name, nameLen, OBJ_VAL(valStr));
            }
        } else {
            if (*p != *u) return false;
            p++;
            u++;
        }
    }
    
    // Both should be at end for exact match
    return (*p == '\0' && *u == '\0');
}

static Value uhttp_route(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount != 3 || !IS_STRING(args[0]) || !IS_STRING(args[1]) || !IS_STRING(args[2])) {
        printf("Error: ucoreHttp.route(method, path, handlerName) expects 3 strings.\n");
        return BOOL_VAL(false);
    }
    
    Route* r = malloc(sizeof(Route));
    r->method = strdup(AS_CSTRING(args[0]));
    r->path = strdup(AS_CSTRING(args[1]));
    r->handlerName = strdup(AS_CSTRING(args[2]));
    r->next = g_routes;
    g_routes = r;
    
    return BOOL_VAL(true);
}

static Value uhttp_listen(VM* vm, Value* args, int argCount) {
    if (argCount < 1 || !IS_INT(args[0])) {
        printf("Error: ucoreHttp.listen(port, [handlerName]) expects int port.\n");
        return BOOL_VAL(false);
    }
    
    int port = AS_INT(args[0]);
    char* mainHandlerName = NULL;
    Function* mainHandler = NULL;
    
    if (argCount >= 2 && IS_STRING(args[1])) {
        mainHandlerName = AS_CSTRING(args[1]);
        mainHandler = findFunctionByName(vm, mainHandlerName);
        if (!mainHandler) {
             printf("Error: Handler function '%s' not found.\n", mainHandlerName);
             return BOOL_VAL(false);
        }
    }
    
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    
    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        return BOOL_VAL(false);
    }
    
    // Attach socket to port
    // SO_REUSEADDR allows restarting server quickly
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) { 
        perror("setsockopt");
        return BOOL_VAL(false);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return BOOL_VAL(false);
    }
    
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        return BOOL_VAL(false);
    }
    
    printf("Server listening on port %d...\n", port);
    if (mainHandlerName) printf("Using main handler: %s\n", mainHandlerName);
    else printf("Using router.\n");
    fflush(stdout);
    
    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }
        
        // Read Request
        char buffer[4096] = {0};
        ssize_t n = recv(new_socket, buffer, 4096, 0);
        if (n <= 0) {
            close(new_socket);
            continue;
        }
        
        // Parse Method and Path (Simple)
        char method[16] = {0};
        char path[1024] = {0};
        char* body = "";
        
        sscanf(buffer, "%s %s", method, path);
        
        // Find body
        char* bodyStart = strstr(buffer, "\r\n\r\n");
        if (bodyStart) body = bodyStart + 4;
        
        // Parse path and query string first (needed for route matching)
        char cleanPath[1024];
        char queryStr[1024] = {0};
        char* queryStart = strchr(path, '?');
        if (queryStart) {
            int pathLen = queryStart - path;
            memcpy(cleanPath, path, pathLen);
            cleanPath[pathLen] = '\0';
            strncpy(queryStr, queryStart + 1, sizeof(queryStr) - 1);
        } else {
            strncpy(cleanPath, path, sizeof(cleanPath) - 1);
            cleanPath[sizeof(cleanPath) - 1] = '\0';
        }
        
        // Try serving static file first
        if (tryServeStatic(new_socket, cleanPath)) {
            close(new_socket);
            continue;
        }
        
        // req.params - will be populated by route matching
        Map* paramsMap = newMap(vm);
        
        // Determine Handler (with pattern matching)
        Function* targetHandler = mainHandler;
        if (!targetHandler) {
            // Check routes
            Route* r = g_routes;
            while (r) {
                if (strcmp(r->method, method) == 0) {
                    // Try pattern matching (supports :param syntax)
                    if (matchRoute(r->path, cleanPath, vm, paramsMap)) {
                        targetHandler = findFunctionByName(vm, r->handlerName);
                        break;
                    }
                }
                r = r->next;
            }
        }
        
        if (!targetHandler) {
            // 404
            const char* msg = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            send(new_socket, msg, strlen(msg), 0);
            close(new_socket);
            continue;
        }
        
        // Build Request Map (Standard)
        Map* reqMap = newMap(vm);
        Value vMethod = OBJ_VAL(internString(vm, method, (int)strlen(method)));
        mapSetStr(reqMap, "method", 6, vMethod);
        
        Value vPath = OBJ_VAL(internString(vm, cleanPath, (int)strlen(cleanPath)));
        mapSetStr(reqMap, "path", 4, vPath);
        
        // req.query - parsed query string
        Map* queryMap = newMap(vm);
        parseQueryString(vm, queryMap, queryStr);
        mapSetStr(reqMap, "query", 5, OBJ_VAL(queryMap));
        
        // req.headers - parsed headers
        Map* headerMap = newMap(vm);
        parseHeaders(vm, headerMap, buffer);
        mapSetStr(reqMap, "headers", 7, OBJ_VAL(headerMap));
        
        // req.params - already populated by route matching
        mapSetStr(reqMap, "params", 6, OBJ_VAL(paramsMap));

        // req.body
        Value vBody = OBJ_VAL(internString(vm, body, (int)strlen(body)));
        mapSetStr(reqMap, "body", 4, vBody);
        
        Value vReq = OBJ_VAL(reqMap);
        
        // Call Handler
        Value handlerArgs[1];
        handlerArgs[0] = vReq;
        
        Value resVal = callFunction(vm, targetHandler, handlerArgs, 1);
        
        // Send Response - Support both string and Map responses
        char respBuffer[8192];
        int statusCode = 200;
        const char* contentType = "text/plain";
        char* content = "";
        bool freeContent = false;
        
        if (IS_STRING(resVal)) {
            // Simple string response
            content = AS_CSTRING(resVal);
        } else if (IS_MAP(resVal)) {
            // Map response: {status: 201, contentType: "application/json", body: "..."}
            Map* resMap = (Map*)AS_OBJ(resVal);
            
            // Get status code
            int bucket;
            MapEntry* statusEntry = mapFindEntry(resMap, "status", 6, &bucket);
            if (statusEntry && IS_INT(statusEntry->value)) {
                statusCode = AS_INT(statusEntry->value);
            }
            
            // Get content type
            MapEntry* typeEntry = mapFindEntry(resMap, "contentType", 11, &bucket);
            if (typeEntry && IS_STRING(typeEntry->value)) {
                contentType = AS_CSTRING(typeEntry->value);
            }
            
            // Get body
            MapEntry* bodyEntry = mapFindEntry(resMap, "body", 4, &bucket);
            if (bodyEntry) {
                if (IS_STRING(bodyEntry->value)) {
                    content = AS_CSTRING(bodyEntry->value);
                } else {
                    // Auto-serialize non-string body to JSON
                    int cap = 1024, len = 0;
                    char* buf = malloc(cap);
                    buf[0] = '\0';
                    json_serialize_val(bodyEntry->value, &buf, &len, &cap);
                    content = buf;
                    freeContent = true;
                    contentType = "application/json";
                }
            }
        }
        
        // Build HTTP response
        const char* statusText = "OK";
        if (statusCode == 201) statusText = "Created";
        else if (statusCode == 204) statusText = "No Content";
        else if (statusCode == 400) statusText = "Bad Request";
        else if (statusCode == 401) statusText = "Unauthorized";
        else if (statusCode == 403) statusText = "Forbidden";
        else if (statusCode == 404) statusText = "Not Found";
        else if (statusCode == 500) statusText = "Internal Server Error";
        
        snprintf(respBuffer, sizeof(respBuffer), 
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %ld\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "\r\n"
            "%s", statusCode, statusText, contentType, strlen(content), content);
            
        send(new_socket, respBuffer, strlen(respBuffer), 0);
        if (freeContent) free(content);
        
        close(new_socket);
    }
    
    return BOOL_VAL(true);
}


void registerUCoreHttp(VM* vm) {
    ObjString* modNameObj = internString(vm, "ucoreHttp", 9);
    char* modName = modNameObj->chars; 
    
    Module* mod = ALLOCATE_OBJ(vm, Module, OBJ_MODULE);
    mod->name = strdup(modName); 
    mod->obj.isMarked = true; 
    mod->obj.isPermanent = true; // PERMANENT ROOT
    
    // CRITICAL: Use ALLOCATE_OBJ for Environment to enable GC tracking
    Environment* modEnv = ALLOCATE_OBJ(vm, Environment, OBJ_ENVIRONMENT);
    memset(modEnv->buckets, 0, sizeof(modEnv->buckets));
    memset(modEnv->funcBuckets, 0, sizeof(modEnv->funcBuckets));
    modEnv->enclosing = NULL; // Globals
    modEnv->obj.isMarked = true; 
    modEnv->obj.isPermanent = true; // PERMANENT ROOT
    mod->env = modEnv;
    
    defineNative(vm, mod->env, "get", uhttp_get, 1);
    defineNative(vm, mod->env, "post", uhttp_post, 2);
    defineNative(vm, mod->env, "put", uhttp_put, 2);
    defineNative(vm, mod->env, "delete", uhttp_delete, 1);
    defineNative(vm, mod->env, "patch", uhttp_patch, 2);
    defineNative(vm, mod->env, "listen", uhttp_listen, 2);
    defineNative(vm, mod->env, "json", uhttp_json, 1);
    defineNative(vm, mod->env, "route", uhttp_route, 3);
    defineNative(vm, mod->env, "use", uhttp_use, 1);
    defineNative(vm, mod->env, "static", uhttp_static, 2);
    
    Value vMod = OBJ_VAL(mod);
    defineGlobal(vm, "ucoreHttp", vMod);
}

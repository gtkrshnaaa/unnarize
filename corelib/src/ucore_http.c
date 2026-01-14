#include "ucore_http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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


// ---- Routing ----
typedef struct Route {
    char* method;
    char* path;
    char* handlerName;
    struct Route* next;
} Route;

static Route* g_routes = NULL;

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
        
        // Determine Handler
        Function* targetHandler = mainHandler;
        if (!targetHandler) {
            // Check routes
            Route* r = g_routes;
            while (r) {
                if (strcmp(r->method, method) == 0 && strcmp(r->path, path) == 0) {
                    targetHandler = findFunctionByName(vm, r->handlerName);
                    break;
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
        
        // Build Request Map
        Map* reqMap = newMap(vm);
        Value vMethod = OBJ_VAL(internString(vm, method, (int)strlen(method)));
        mapSetStr(reqMap, "method", 6, vMethod);
        
        Value vPath = OBJ_VAL(internString(vm, path, (int)strlen(path)));
        mapSetStr(reqMap, "path", 4, vPath);

        Value vBody = OBJ_VAL(internString(vm, body, (int)strlen(body)));
        mapSetStr(reqMap, "body", 4, vBody);
        
        Value vReq = OBJ_VAL(reqMap);
        
        // Call Handler
        Value handlerArgs[1];
        handlerArgs[0] = vReq;
        
        Value resVal = callFunction(vm, targetHandler, handlerArgs, 1);
        
        // Send Response
        char respBuffer[4096];
        char* content = "";
        if (IS_STRING(resVal)) {
            content = AS_CSTRING(resVal);
        }
        
        snprintf(respBuffer, sizeof(respBuffer), 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %ld\r\n"
            "\r\n"
            "%s", strlen(content), content);
            
        send(new_socket, respBuffer, strlen(respBuffer), 0);
        
        close(new_socket);
    }
    
    return BOOL_VAL(true);
}


void registerUCoreHttp(VM* vm) {
    ObjString* modNameObj = internString(vm, "ucoreHttp", 9);
    char* modName = modNameObj->chars; 
    
    Module* mod = ALLOCATE_OBJ(vm, Module, OBJ_MODULE);
    mod->name = strdup(modName); 
    mod->obj.isMarked = true; // PERMANENT ROOT
    
    // CRITICAL: Use ALLOCATE_OBJ for Environment to enable GC tracking
    Environment* modEnv = ALLOCATE_OBJ(vm, Environment, OBJ_ENVIRONMENT);
    memset(modEnv->buckets, 0, sizeof(modEnv->buckets));
    memset(modEnv->funcBuckets, 0, sizeof(modEnv->funcBuckets));
    modEnv->enclosing = NULL; // Globals
    modEnv->obj.isMarked = true; // PERMANENT ROOT
    mod->env = modEnv;
    
    defineNative(vm, mod->env, "get", uhttp_get, 1);
    defineNative(vm, mod->env, "post", uhttp_post, 2);
    defineNative(vm, mod->env, "listen", uhttp_listen, 2);
    defineNative(vm, mod->env, "json", uhttp_json, 1);
    defineNative(vm, mod->env, "route", uhttp_route, 3);
    
    Value vMod = OBJ_VAL(mod);
    defineGlobal(vm, "ucoreHttp", vMod);
}

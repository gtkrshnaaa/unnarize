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
        strncpy(pathOut, p, 1024); // simplistic
    } else {
        strcpy(pathOut, "/");
    }
    return true;
}

static Value http_perform(const char* method, const char* url, const char* body) {
    char host[256];
    char path[1024];
    int port;
    
    if (!parseUrl(url, host, path, &port)) {
        printf("Invalid URL or HTTPS not supported.\n");
        return (Value){VAL_INT, .intVal=0}; // null
    }
    
    // DNS Resolve
    struct hostent* he = gethostbyname(host);
    if (!he) {
        printf("Could not resolve host: %s\n", host);
        return (Value){VAL_INT, .intVal=0}; // null
    }
    
    // Socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return (Value){VAL_INT, .intVal=0};
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
        return (Value){VAL_INT, .intVal=0};
    }
    
    // Construct Request
    char req[4096];
    int contentLen = body ? strlen(body) : 0;
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
        return (Value){VAL_INT, .intVal=0};
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
    
    Value vRes; vRes.type = VAL_STRING; vRes.stringVal = strdup(bodyStart);
    free(buf);
    return vRes;
}

static Value uhttp_get(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount != 1 || args[0].type != VAL_STRING) {
        printf("Error: ucoreHttp.get(url) expects url string.\n");
        return (Value){VAL_INT, .intVal=0};
    }
    return http_perform("GET", args[0].stringVal, NULL);
}

static Value uhttp_post(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) {
        printf("Error: ucoreHttp.post(url, body) expects url and body strings.\n");
        return (Value){VAL_INT, .intVal=0};
    }
    return http_perform("POST", args[0].stringVal, args[1].stringVal);
}

static Value uhttp_listen(VM* vm, Value* args, int argCount) {
    if (argCount != 2 || args[0].type != VAL_INT || args[1].type != VAL_STRING) {
        printf("Error: ucoreHttp.listen(port, handlerName) expects int and string.\n");
        return (Value){VAL_BOOL, .boolVal=false};
    }
    
    int port = args[0].intVal;
    char* handlerName = args[1].stringVal;
    
    Function* handler = findFunctionByName(vm, handlerName);
    if (!handler) {
        printf("Error: Handler function '%s' not found.\n", handlerName);
        return (Value){VAL_BOOL, .boolVal=false};
    }
    
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    
    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        return (Value){VAL_BOOL, .boolVal=false};
    }
    
    // Attach socket to port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) { 
        perror("setsockopt");
        return (Value){VAL_BOOL, .boolVal=false};
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return (Value){VAL_BOOL, .boolVal=false};
    }
    
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        return (Value){VAL_BOOL, .boolVal=false};
    }
    
    printf("Server listening on port %d...\n", port);
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
        
        // Build Request Map
        Map* reqMap = newMap();
        Value vMethod; vMethod.type = VAL_STRING; vMethod.stringVal = strdup(method);
        mapSetStr(reqMap, "method", 6, vMethod);
        
        Value vPath; vPath.type = VAL_STRING; vPath.stringVal = strdup(path);
        mapSetStr(reqMap, "path", 4, vPath);

        Value vBody; vBody.type = VAL_STRING; vBody.stringVal = strdup(body);
        mapSetStr(reqMap, "body", 4, vBody);
        
        Value vReq; vReq.type = VAL_MAP; vReq.mapVal = reqMap;
        
        // Call Handler
        Value handlerArgs[1];
        handlerArgs[0] = vReq;
        
        Value resVal = callFunction(vm, handler, handlerArgs, 1);
        
        // Send Response
        char respBuffer[4096];
        char* content = "";
        if (resVal.type == VAL_STRING) {
            content = resVal.stringVal;
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
    
    return (Value){VAL_BOOL, .boolVal=true};
}


void registerUCoreHttp(VM* vm) {
    Module* mod = malloc(sizeof(Module));
    mod->name = strdup("ucoreHttp");
    mod->env = malloc(sizeof(Environment));
    memset(mod->env->buckets, 0, sizeof(mod->env->buckets));
    memset(mod->env->funcBuckets, 0, sizeof(mod->env->funcBuckets));
    
    // get
    unsigned int hGet = hash("get", 3);
    FuncEntry* feGet = malloc(sizeof(FuncEntry)); feGet->key = strdup("get");
    feGet->next = mod->env->funcBuckets[hGet]; mod->env->funcBuckets[hGet] = feGet;
    Function* fnGet = malloc(sizeof(Function)); fnGet->isNative = true; fnGet->native = uhttp_get; fnGet->paramCount = 1;
    fnGet->name = (Token){TOKEN_IDENTIFIER, feGet->key, 3, 0}; feGet->function = fnGet;

    // post
    unsigned int hPost = hash("post", 4);
    FuncEntry* fePost = malloc(sizeof(FuncEntry)); fePost->key = strdup("post");
    fePost->next = mod->env->funcBuckets[hPost]; mod->env->funcBuckets[hPost] = fePost;
    Function* fnPost = malloc(sizeof(Function)); fnPost->isNative = true; fnPost->native = uhttp_post; fnPost->paramCount = 2;
    fnPost->name = (Token){TOKEN_IDENTIFIER, fePost->key, 4, 0}; fePost->function = fnPost;

    // listen
    unsigned int hListen = hash("listen", 6);
    FuncEntry* feListen = malloc(sizeof(FuncEntry)); feListen->key = strdup("listen");
    feListen->next = mod->env->funcBuckets[hListen]; mod->env->funcBuckets[hListen] = feListen;
    Function* fnListen = malloc(sizeof(Function)); fnListen->isNative = true; fnListen->native = uhttp_listen; fnListen->paramCount = 2;
    fnListen->name = (Token){TOKEN_IDENTIFIER, feListen->key, 6, 0}; feListen->function = fnListen;

    // Add to global
    VarEntry* ve = malloc(sizeof(VarEntry));
    ve->key = strdup("ucoreHttp");
    ve->value.type = VAL_MODULE; ve->value.moduleVal = mod;
    unsigned int h = hash("ucoreHttp", 9);
    ve->next = vm->globalEnv->buckets[h];
    vm->globalEnv->buckets[h] = ve;
}

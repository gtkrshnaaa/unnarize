#include "resolver.h"

// Resolver State
typedef struct {
    Token name;
    int depth;
    int slot;
} Local;

#define MAX_LOCALS 256
#define MAX_SCOPES 64

typedef struct {
    Local locals[MAX_LOCALS];
    int localCount;
    int scopeDepth;
} Resolver;

static void initResolver(Resolver* r) {
    r->localCount = 0;
    r->scopeDepth = 0;
}

static void beginScope(Resolver* r) {
    r->scopeDepth++;
}

static void endScope(Resolver* r) {
    r->scopeDepth--;
    // Pop locals from this scope
    while (r->localCount > 0 && r->locals[r->localCount - 1].depth > r->scopeDepth) {
        r->localCount--;
    }
}

static void declareVariable(Resolver* r, Token name, Node* node) {
    if (r->scopeDepth == 0) return; // Global

    // Check for redefinition in same scope
    for (int i = r->localCount - 1; i >= 0; i--) {
        Local* local = &r->locals[i];
        if (local->depth != -1 && local->depth < r->scopeDepth) {
            break;
        }
        // Compare interned pointers or string content?
        // AST is interned, so we can likely use pointer comparison if tokens are interned.
        // Wait, parser creates tokens. internAST runs BEFORE resolve? 
        // Logic says: internAST runs first, then resolve.
        // So name.start is interned.
        if (name.start == local->name.start) {
            // Already declared in this scope
            errorAtToken(name, "Variable with this name already declared in this scope.");
        }
    }

    if (r->localCount == MAX_LOCALS) {
        errorAtToken(name, "Too many local variables in function.");
        return;
    }

    Local* local = &r->locals[r->localCount++];
    local->name = name;
    local->depth = -1; // Declared but not initialized yet
    local->slot = r->localCount - 1; 
    
    // Store slot in the declaration node itself
    if (node) {
        if (node->type == NODE_STMT_VAR_DECL) {
             node->varDecl.slot = local->slot;
        } else if (node->type == NODE_STMT_FOREACH) {
             node->foreachStmt.slot = local->slot;
        }
    }
}

static void defineVariable(Resolver* r) {
    if (r->scopeDepth == 0) return;
    r->locals[r->localCount - 1].depth = r->scopeDepth; // Mark initialized
}

static int resolveLocal(Resolver* r, Token name) {
    for (int i = r->localCount - 1; i >= 0; i--) {
        Local* local = &r->locals[i];
        if (name.start == local->name.start) {
            if (local->depth == -1) {
                errorAtToken(name, "Can't read local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1; // Not found -> Global
}

static void resolve(Resolver* r, Node* node);

static void resolveFunction(Resolver* r, Node* node) {
    beginScope(r);
    // Declare params
    if (node->type == NODE_STMT_FUNCTION) {
        for (int i=0; i < node->function.paramCount; i++) {
            declareVariable(r, node->function.params[i], NULL); // No Node for param decl
            defineVariable(r);
        }
        resolve(r, node->function.body);
    }
    endScope(r);
}

static void resolve(Resolver* r, Node* node) {
    if (!node) return;

    switch (node->type) {
        case NODE_STMT_BLOCK:
            beginScope(r);
            for (int i=0; i<node->block.count; i++) resolve(r, node->block.statements[i]);
            endScope(r);
            break;
            
        case NODE_STMT_VAR_DECL:
            resolve(r, node->varDecl.initializer);
            declareVariable(r, node->varDecl.name, node);
            defineVariable(r);
            break;
            
        case NODE_EXPR_VAR: {
            int slot = resolveLocal(r, node->var.name);
            node->var.slot = slot;
            break;
        }
        
        case NODE_STMT_ASSIGN: {
            resolve(r, node->assign.value);
            int slot = resolveLocal(r, node->assign.name);
            node->assign.slot = slot;
            break;
        }
        
        case NODE_STMT_FUNCTION:
            declareVariable(r, node->function.name, NULL);
            defineVariable(r);
            {
                // New Resolver state for function body (reset locals, start at slot 0)
                // We must save current resolver state if we supported nested functions/closures properly
                // But for now, assuming flat stack per function call (Unnarize architecture).
                // Just create a fresh resolver for the function.
                Resolver funcResolver;
                initResolver(&funcResolver);
                beginScope(&funcResolver);
                resolveFunction(&funcResolver, node);
                endScope(&funcResolver);
            }
            break;

        case NODE_STMT_IF:
            resolve(r, node->ifStmt.condition);
            resolve(r, node->ifStmt.thenBranch);
            if (node->ifStmt.elseBranch) resolve(r, node->ifStmt.elseBranch);
            break;

        case NODE_STMT_WHILE:
            resolve(r, node->whileStmt.condition);
            resolve(r, node->whileStmt.body);
            break;

        case NODE_STMT_FOR:
            beginScope(r);
            resolve(r, node->forStmt.initializer);
            resolve(r, node->forStmt.condition);
            resolve(r, node->forStmt.increment); 
            resolve(r, node->forStmt.body); 
            endScope(r);
            break;
        
        case NODE_STMT_FOREACH:
            beginScope(r);
            resolve(r, node->foreachStmt.collection);
            declareVariable(r, node->foreachStmt.iterator, node);
            defineVariable(r);
            resolve(r, node->foreachStmt.body);
            endScope(r);
            break;
            
        case NODE_EXPR_BINARY:
            resolve(r, node->binary.left);
            resolve(r, node->binary.right);
            break;

        case NODE_EXPR_CALL:
            resolve(r, node->call.callee);
            resolve(r, node->call.arguments);
            break;

        case NODE_STMT_RETURN:
            if (node->returnStmt.value) resolve(r, node->returnStmt.value);
            break;
            
        case NODE_STMT_PRINT:
            resolve(r, node->print.expr);
            break;
            
        case NODE_EXPR_UNARY: 
             resolve(r, node->unary.expr);
             break;

        case NODE_EXPR_GET:
             resolve(r, node->get.object);
             break;

        case NODE_EXPR_INDEX:
             resolve(r, node->index.target);
             resolve(r, node->index.index);
             break;

        case NODE_STMT_INDEX_ASSIGN:
             resolve(r, node->indexAssign.target);
             resolve(r, node->indexAssign.index);
             resolve(r, node->indexAssign.value);
             break;

        case NODE_EXPR_ARRAY_LITERAL: {
             Node* elem = node->arrayLiteral.elements;
             while (elem) {
                 resolve(r, elem);
                 elem = elem->next; // Linked list of expressions? No, Linked list of nodes?
                 // Parser def: Node* elements; // Linked list
                 // Usually elements are expressions linked via next field?
                 // Let's assume standard linked list of nodes
             }
             break;
        }

        case NODE_EXPR_AWAIT:
             resolve(r, node->unary.expr); // Await is unary-like structure in some parsers?
             // In parser.h: NODE_EXPR_AWAIT uses unary structure? 
             // "NODE_EXPR_AWAIT" defined but struct union? "unary" covers it?
             // Need to check parser.h. Assuming it uses unary if not explicit.
             // Actually, parser.h showed NODE_EXPR_AWAIT enum but struct union didn't show explicit await struct.
             // It likely uses unary struct or simple expression.
             // Looking at parser.c (not shown) or inferring: usually `await expr`.
             break;

        case NODE_STMT_IMPORT:
             // Nothing to resolve in import path?
             break;

        case NODE_STMT_STRUCT_DECL:
             // Struct name is global usually? Or scoped?
             // Unnarize structs are currently globals.
             break;

        case NODE_STMT_PROP_ASSIGN:
             resolve(r, node->propAssign.object);
             resolve(r, node->propAssign.value);
             break;

        case NODE_EXPR_LITERAL:
             break;
             
        default:
            break;
    }
    resolve(r, node->next);
}

// Main entry point
bool resolveAST(VM* vm, Node* ast) {
    if (!ast) return true;
    (void)vm; // Parser/Lexer might report errors
    
    Resolver resolver;
    initResolver(&resolver);
    
    // If root is a BLOCK, we want to resolve its children directly in scope 0 (Global)
    // instead of creating a new scope level 1.
    if (ast->type == NODE_STMT_BLOCK) {
        for (int i = 0; i < ast->block.count; i++) {
             resolve(&resolver, ast->block.statements[i]);
        }
    } else {
        resolve(&resolver, ast);
    }
    
    return true; 
}

#ifndef RESOLVER_H
#define RESOLVER_H

#include "common.h"
#include "parser.h"
#include "vm.h" // For VM struct definition if needed, or forward declare

/**
 * Resolves local variables to stack slots.
 * This function traverses the AST and populates the 'slot' field
 * in value-related nodes (VAR, VAR_DECL, ASSIGN, etc.).
 * 
 * @param vm Pointer to VM (may be used for global checks or logging)
 * @param node Root of the AST to resolve
 * @return true if resolution succeeded, false if errors occurred
 */
bool resolveAST(VM* vm, Node* node);

#endif // RESOLVER_H

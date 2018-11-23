#ifndef NDEBUG

#include "branch.h"
#include <stdlib.h>
#include "info.h"
#include "util.h"

static void _debug_branch(branch br, int r)
{
	if (br.flags & BRANCH_ISLEAF) {
		switch (br.flags & BRANCH_TYPE_MASK) {
		case BRANCH_TYPE_VAR:
		{
			info_var *i = br.ptr; 
			debug("%*cVAR '%s' '%s'", r, ' ', i->type ? i->type->buf : NULL, i->name->buf);
			debug_expr(i->expr, r + 2);
		}
		break;
		case BRANCH_TYPE_C:
		{
			info_for *i = br.ptr;
			switch(br.flags & BRANCH_CTYPE_MASK) {
			case BRANCH_CTYPE_WHILE: debug("%*cWHILE", r, ' '); break;
			case BRANCH_CTYPE_IF:    debug("%*cIF"   , r, ' '); break;
			case BRANCH_CTYPE_ELSE:  debug("%*cELSE" , r, ' '); return;
				case BRANCH_CTYPE_RET:   debug("%*cRET"  , r, ' '); break;
			case BRANCH_CTYPE_FOR:
				debug("%*cFOR   '%s' '%s'", r, ' ', i->var, i->action);
				debug_expr(i->expr, r + 2);
					return;
			default:
				debug("Invalid control type flag 0x%x", br.flags);
				exit(1);
			}
			debug_expr(*br.expr, r + 2);
		}
		break;
		default:
		{
			debug("Invalid type flag 0x%x", br.flags);
			exit(1);
		}
		}
	} else {
		for (size_t i = 0; i < br.len; i++)
			_debug_branch(br.branches[i], r + 2);
	}
}

void debug_branch(branch br)
{
	_debug_branch(br, 2);
}

#endif

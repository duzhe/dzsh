#include "command.h"
#include "mempool.h"
#include "list.h"

struct command *create_command(struct mempool *pool)
{
	struct command *cmd;
	cmd = p_alloc(pool, sizeof(struct command));
	cmd->params = l_create(pool);
	cmd->redirections = l_create(pool);
	cmd->bin = NULL;
	return cmd;
}


BOOL command_empty(struct command *cmd)
{
	return l_empty(cmd->params) && l_empty(cmd->redirections);
}


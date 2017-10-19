#include "command.h"
#include "mempool.h"
#include "list.h"

struct simple_command *create_command(struct mempool *pool)
{
	struct simple_command *cmd;
	cmd = p_alloc(pool, sizeof(struct simple_command));
	cmd->type = CMD_TYPE_SIMPLE;
	cmd->envp = l_create(pool);
	cmd->params = l_create(pool);
	cmd->redirections = l_create(pool);
	cmd->bin = NULL;
	return cmd;
}


struct complex_command *create_complex_command(struct mempool *pool, cmdtype type)
{
	struct complex_command *cmd;
	cmd = p_alloc(pool, sizeof(struct complex_command));
	cmd->type = type;
	cmd->commands = l_create(pool);
	return cmd;
}


struct complex_command *create_pipe_command(struct mempool *pool)
{
	return create_complex_command(pool, CMD_TYPE_PIPE);
}


struct complex_command *create_logic_command(struct mempool *pool)
{
	return create_complex_command(pool, CMD_TYPE_LOGIC);
}


BOOL command_empty(struct simple_command *cmd)
{
	return l_empty(cmd->params) && l_empty(cmd->redirections);
}


cmdseperator command_get_seperator(struct command *cmd)
{
	struct command *c;

	if (cmd->type == CMD_TYPE_SIMPLE) {
		return ((struct simple_command *)cmd)->sep;
	}
	c = ((struct complex_command *)cmd)->commands->last->data;
	return command_get_seperator(c);
}


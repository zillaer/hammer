#include <stdio.h>
#include "hammer_connection.h"
#include "hammer_macros.h"
#include "hammer_list.h"
#include "hammer_sched.h"
#include "hammer_memory.h"
#include "hammer_config.h"
#include "hammer_socket.h"
#include "libpool.h"

extern hammer_config_t *config;

hammer_job_t *hammer_job_get()
{
#if defined(HAMMER_MALLOC)
	return (hammer_job_t *)hammer_mem_malloc(sizeof(hammer_job_t));
#else
	hammer_sched_t *sched = hammer_sched_get_sched_struct();
	return (hammer_job_t *)libpool_alloc(MEMPOOL_SIZE_JOB, sched->thread_id);
#endif
}

int hammer_job_free(hammer_job_t *job)
{
#if defined(HAMMER_MALLOC)
	hammer_mem_free(job);
#else
	hammer_sched_t *sched = hammer_sched_get_sched_struct();
	libpool_free(job, MEMPOOL_SIZE_JOB, sched->thread_id);
#endif
	return 0;
}

int hammer_conn_job_add(hammer_connection_t *c, int length)
{
	hammer_job_t *new_job = hammer_job_get();
	
	new_job->job_body_ptr = c->body_ptr + c->body_length;
	new_job->job_body_length = length;
	new_job->connection = c;

	hammer_list_add(&(new_job->_head), c->job_list);

	c->body_length += length;

	return 0;
}

int hammer_conn_job_del(hammer_job_t *job)
{
	hammer_list_del(&(job->_head));
	hammer_job_free(job);

	return 0;
}


void hammer_conn_init()
{

}


// init a connection struct
void hammer_init_connection(hammer_connection_t *c)
{
	c->socket = 0;
	c->body_ptr = hammer_mem_malloc(config->conn_buffer_size);
	c->body_size = config->conn_buffer_size;
	c->body_length = 0;
	c->rc = NULL;
	c->job_list = NULL;

	return;
}

hammer_connection_t *hammer_get_connection()
{
	hammer_connection_t *c;
#if defined(HAMMER_MALLOC)
	c = hammer_mem_malloc(sizeof(hammer_connection_t));
#else
	hammer_sched_t *sched = hammer_sched_get_sched_struct();
	c = (hammer_connection_t *)libpool_alloc(MEMPOOL_SIZE_CONN, sched->thread_id);
#endif
	hammer_init_connection(c);

	return c;
}

void hammer_free_connection(hammer_connection_t *c)
{
#if defined(HAMMER_MALLOC)
	hammer_mem_free(c);
#else
	hammer_sched_t *sched = hammer_sched_get_sched_struct();
	libpool_free(c, MEMPOOL_SIZE_CONN, sched->thread_id);
#endif
	return;
}

int hammer_close_connection(hammer_connection_t *c)
{
	hammer_job_t *this_job;
	struct hammer_list *job_list, *job_head;

	if (c == NULL) {
		hammer_err("c is null\n");
		return 0;
	}

	hammer_socket_close(c->socket);

	job_list = c->job_list;
	hammer_list_foreach(job_head, job_list) {
		this_job = hammer_list_entry(job_head, hammer_job_t, _head);
		hammer_conn_job_del(this_job);
	}

	hammer_free_connection(c);

	return 0;
}


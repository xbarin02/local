#define _XOPEN_SOURCE 500
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

struct task {
	char *out;
	char *err;
	char *cwd;
	int argc;
	char **argv;
};

char *strdup0(const char *s)
{
	return s != NULL ? strdup(s) : NULL;
}

struct task task_create(char *out, char *err, char *cwd, int argc, char *argv[])
{
	struct task t;

	t.out = strdup0(out);
	t.err = strdup0(err);
	t.cwd = strdup0(cwd);

	t.argc = argc + 1;

	t.argv = malloc(t.argc * sizeof(char *));

	if (t.argv != NULL) {
		for (int i = 0; i < argc; ++i) {
			t.argv[i] = strdup(argv[i]);
		}
	}

	t.argv[argc] = NULL;

	return t;
}

void task_destroy(struct task t)
{
	free(t.out);
	free(t.err);
	free(t.cwd);

	if (t.argv != NULL) {
		for (int i = 0; i < t.argc; ++i) {
			free(t.argv[i]);
		}
	}

	free(t.argv);
}

static int slots = 4;

static struct queue {
	pid_t pid;
	struct task t;
	struct queue *lchild, *rchild;
} *root = NULL;

void queue_push_(pid_t pid, struct task t, struct queue **node)
{
	if (*node == NULL) {
		*node = malloc(sizeof(struct queue));
		if (*node == NULL) {
			abort();
		}
		(*node)->pid = pid;
		(*node)->t = t;
		(*node)->lchild = NULL;
		(*node)->rchild = NULL;
		return;
	}

	if (pid < (*node)->pid) {
		queue_push_(pid, t, &(*node)->lchild);
	} else {
		queue_push_(pid, t, &(*node)->rchild);
	}
}

void queue_push(pid_t pid, struct task t)
{
	queue_push_(pid, t, &root);
}

void printf_task(struct task t)
{
	for (int i = 0; i < t.argc; ++i) {
		if (t.argv[i] != NULL) {
			printf(" %s", t.argv[i]);
		}
	}
}

void queue_dump_(struct queue *node)
{
	if (node != NULL) {
		printf("R %lu", (unsigned long)node->pid);
		printf_task(node->t);
		printf("\n");

		queue_dump_(node->lchild);
		queue_dump_(node->rchild);
	}
}

void queue_dump()
{
// 	printf("Running tasks:\n");
// 	printf("==============\n");

	queue_dump_(root);
}

pid_t task_exec(struct task t)
{
	pid_t pid = fork();

	if (pid == -1) {
		perror("Cannot fork the master\n");
		return -1;
	}

	if (pid == 0) {
		/* the child process */
		/* TODO: exec the slave */
		execvp(t.argv[0], t.argv);
		perror("Cannot run the requested command");
		return -1;
	} else {
		/* the parent process */
		queue_push(pid, t);
		return pid;
	}
}

/* task queue */
struct taskq {
	struct task t;
	struct taskq *next;
} *tq = NULL;

void task_submit(struct task t)
{
	struct taskq **q = &tq;

	while (*q != NULL) {
		q = &(*q)->next;
	}

	*q = malloc(sizeof(struct taskq));

	if (*q == NULL) {
		abort();
	}

	(*q)->t = t;
	(*q)->next = NULL;
}

void task_dump()
{
// 	size_t qs = 0;
	struct taskq *q = tq;

// 	printf("Tasks in queue:\n");
// 	printf("===============\n");

	while (q != NULL) {
		printf("Q");
		printf_task(q->t);
		printf("\n");

// 		qs++;
		q = q->next;
	}

// 	printf("===============\n");
// 	printf("Total tasks in queue: %zu\n", qs);
}

int exec_tasks()
{
	while (slots > 0 && tq != NULL) {
		struct task t = tq->t;
		struct taskq *tmp = tq;
		tq = tq->next;
		free(tmp);
		if (task_exec(t) == -1) {
			return -1;
		}
		--slots;
	}

	return 0;
}

void queue_add_(struct queue **node, struct queue *child)
{
	if (*node == NULL) {
		*node = child;
	} else {
		if (child != NULL) {
			if ((*node)->pid < child->pid) {
				queue_add_(&(*node)->lchild, child);
			} else {
				queue_add_(&(*node)->rchild, child);
			}
		}
	}
}

/*
 * returns 1 if the queue has been modified
 */
int queue_pop_(struct queue **node)
{
	if (*node != NULL) {
		int status;

		pid_t pid = waitpid((*node)->pid, &status, WNOHANG);
		if (pid == -1 || pid == (*node)->pid) {
			/* process already removed, or right now removed */
			++slots;
			task_destroy((*node)->t);
			struct queue *lchild = (*node)->lchild;
			struct queue *rchild = (*node)->rchild;
			free(*node);
			*node = NULL;
			queue_add_(&root, lchild);
			queue_add_(&root, rchild);
			return 1;
		} else {
			if (queue_pop_(&(*node)->lchild) == 1) {
				return 1;
			}
			if (queue_pop_(&(*node)->rchild) == 1) {
				return 1;
			}
			return 0;
		}
	}
	return 0;
}

void queue_pop()
{
	while (queue_pop_(&root) == 1)
		;
}

void wait_for_task()
{
	int status;

	wait(&status);
}

void dump()
{
	printf("Tasks:\n");
	printf("======\n");
	queue_dump();
	task_dump();
	printf("\n");
}

int main(/*int argc, char *argv[]*/)
{
	char *a[] = { "sleep", "2", NULL };

	for (int i = 0; i < 10; ++i) {
		task_submit(task_create(NULL, NULL, NULL, 2, a));
	}

	while (1) {
		queue_pop();
		if (exec_tasks() == -1) {
			return -1;
		}
		dump();
		sleep(1);
	}

	return 0;
}

// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>

#include "skiplist_validate.h"

static unsigned int passed;
static unsigned int failed;

static struct sl_node *test_node(u64 key, unsigned int height)
{
	struct sl_node *node;

	node = kzalloc(sizeof(*node) + height * sizeof(node->next[0]),
				   GFP_KERNEL);
	if (!node)
		return NULL;

	node->key = key;
	node->height = height;
	return node;
}

static void check_result(const char *name, int actual, int expected)
{
	if (actual == expected) {
		passed++;
	} else {
		failed++;
		pr_err("sl_validate_test: %s: got %d, expected %d\n",
			   name, actual, expected);
	}
}

static int __init validate_test_init(void)
{
	struct sl_node *head, *a, *b, *c;
	int ret = -ENOMEM;

	head = test_node(0, SL_MAX_LEVEL);
	a = test_node(10, 2);
	b = test_node(20, 1);
	c = test_node(30, 3);
	if (!head || !a || !b || !c)
		goto out;

	check_result("empty", sl_validate(head, 0), 0);
	check_result("null head", sl_validate(NULL, 0), -EINVAL);

	head->height = 1;
	check_result("bad head height", sl_validate(head, 0), -EINVAL);
	head->height = SL_MAX_LEVEL;

	head->key = 1;
	check_result("bad head key", sl_validate(head, 0), -EINVAL);
	head->key = 0;

	/*
	 * L2: head -> c
	 * L1: head -> a -> c
	 * L0: head -> a -> b -> c
	 */
	head->next[0] = a;
	a->next[0] = b;
	b->next[0] = c;
	head->next[1] = a;
	a->next[1] = c;
	head->next[2] = c;

	check_result("valid list", sl_validate(head, 3), 0);
	check_result("count too small", sl_validate(head, 2), -EINVAL);
	check_result("count too large", sl_validate(head, 4), -EINVAL);

	b->key = 10;
	check_result("duplicate key", sl_validate(head, 3), -EINVAL);
	b->key = 5;
	check_result("descending keys", sl_validate(head, 3), -EINVAL);
	b->key = 20;

	a->key = 0;
	check_result("reserved zero", sl_validate(head, 3), -EINVAL);
	a->key = 10;

	c->key = ~(u64)0;
	check_result("reserved maximum", sl_validate(head, 3), -EINVAL);
	c->key = 30;

	a->height = 0;
	check_result("zero height", sl_validate(head, 3), -EINVAL);
	a->height = SL_MAX_LEVEL + 1;
	check_result("excessive height", sl_validate(head, 3), -EINVAL);
	a->height = 2;

	a->next[1] = NULL;
	check_result("missing upper node", sl_validate(head, 3), -EINVAL);
	a->next[1] = c;

	head->next[1] = b;
	check_result("wrong upper node", sl_validate(head, 3), -EINVAL);
	head->next[1] = a;

	c->next[2] = c;
	check_result("upper cycle", sl_validate(head, 3), -EINVAL);
	c->next[2] = NULL;

	c->next[0] = a;
	check_result("level zero cycle", sl_validate(head, 3), -EINVAL);
	c->next[0] = NULL;

	check_result("restored list", sl_validate(head, 3), 0);

	pr_info("sl_validate_test: passed=%u failed=%u\n", passed, failed);
	ret = failed ? -EINVAL : 0;

out:
	/* Free allocations directly, independent of the tested links. */
	kfree(c);
	kfree(b);
	kfree(a);
	kfree(head);
	return ret;
}

static void __exit validate_test_exit(void)
{
}

module_init(validate_test_init);
module_exit(validate_test_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Skip list structural validator tests");

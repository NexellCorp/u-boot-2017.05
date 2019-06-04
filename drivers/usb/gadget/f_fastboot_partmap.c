/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <config.h>
#include <common.h>
#include <errno.h>
#include <fastboot.h>
#include <malloc.h>
#include <asm/io.h>
#include <linux/err.h>

#include "f_fastboot_partmap.h"

/* support fs type */
static struct fastboot_part_name {
	char *name;
	enum fb_part_type type;
} f_part_names[] = {
	{ "bootsector", FASTBOOT_PART_BOOT },
	{ "raw", FASTBOOT_PART_RAW },
	{ "partition", PART_TYPE_PARTITION },
};

static LIST_HEAD(f_dev_head);
static bool f_dev_binded;

static int saveenv(void)
{
#ifdef CONFIG_ENV_IS_NOWHERE
	return 0;
#else
	return run_command("saveenv", 0);
#endif
}

static void parse_comment(const char *str, int len)
{
	char *p = (char *)str;
	char *t = kzalloc(len, GFP_KERNEL);
	char *s = t;

	do {
		char *r = strchr(p, '#');

		if (!r) {
			strncpy(t, p, len - (p - str));
			break;
		}
		strncpy(t, p, (r - p));

		t += (r - p);
		p = strchr(++r, '\n');
		if (!p) {
			printf("---- not end comments '#' ----\n");
			break;
		}
		p++;
	} while ((p - str) > 1);

	memcpy((void *)str, s, len);
	kfree(s);
}

static int parse_string(const char *s, const char *e, char *b, int len)
{
	int l, a = 0;

	do {
		while (0x20 == *s || 0x09 == *s || 0x0a == *s)
			s++;
	} while (0);

	if (0x20 == *(e - 1) || 0x09 == *(e - 1))
		do {
			e--; while (0x20 == *e || 0x09 == *e) { e--; }; a = 1;
		} while (0);

	l = (e - s + a);
	if (len < l) {
		printf("-- Not enough buffer %d for string len %d [%s] --\n",
		       len, l, s);
		return -EINVAL;
	}

	strncpy(b, s, l);
	b[l] = 0;

	return l;
}

static inline void sort_string(char *p, int len)
{
	int i, j;

	for (i = 0, j = 0; i < len; i++) {
		if (0x20 != p[i] && 0x09 != p[i] && 0x0A != p[i])
			p[j++] = p[i];
	}
	p[j] = 0;
}

static int parse_seq_device(const char *parts, const char **ret,
			    struct fb_part_par *f_part)
{
	struct list_head *fd_head = &f_dev_head;
	struct fb_part_dev *fd;
	struct fb_part_par *fp = f_part;
	const char *p, *c, *id = parts;
	char str[32];
	int id_len;

	p = strchr(id, ':');
	if (!p) {
		printf("no <dev-id> identifier\n");
		return 1;
	}
	id_len = p - id;

	/* for next */
	p++, *ret = p;

	c = strchr(id, ',');
	parse_string(id, c, str, sizeof(str));

	list_for_each_entry(fd, fd_head, list) {
		if (!strcmp(fd->device, str)) {
			/* add to device */
			list_add_tail(&fp->link, &fd->part_list);

			/* dev no */
			debug("device: %s", fd->device);
			p = strchr(id, ',');
			if (!p) {
				printf("no <dev-no> identifier\n");
				return -EINVAL;
			}
			p++;
			parse_string(p, p + id_len, str, sizeof(str));
			/* dev no */
			fp->dev_no = simple_strtoul(str, NULL, 10);
			if (fp->dev_no >= fd->dev_max) {
				printf("** Over dev-no max %s.%d : %d **\n",
				       fd->device, fd->dev_max - 1,
				       fp->dev_no);
				return -EINVAL;
			}

			debug(".%d\n", fp->dev_no);
			fp->fd = fd;
			return 0;
		}
	}

	strcpy(fp->name, "unknown");
	list_add_tail(&fp->link, &fd->part_list);

	printf("** Can't device parse : %s **\n", parts);
	return -EINVAL;
}

static int parse_seq_partition(const char *parts, const char **ret,
			       struct fb_part_par *f_part)
{
	struct list_head *fd_head = &f_dev_head;
	struct fb_part_dev *fd;
	struct fb_part_par *fp;
	const char *p, *id = parts;
	char str[32] = { 0, };

	p = strchr(id, ':');
	if (!p) {
		printf("no <name> identifier\n");
		return -EINVAL;
	}

	/* for next */
	p++, *ret = p;
	p--; parse_string(id, p, str, sizeof(str));

	/* check partition */
	list_for_each_entry(fd, fd_head, list) {
		list_for_each_entry(fp, &fd->part_list, link) {
			if (!strcmp(fp->name, str)) {
				printf("** Exist partition : %s -> %s **\n",
				       fd->device, fp->name);
				return -EINVAL;
			}
		}
	}

	/* set partition name */
	strcpy(f_part->name, str);
	f_part->name[strlen(str)] = 0;

	debug("part  : %s\n", f_part->name);
	return 0;
}

static int parse_seq_fs(const char *parts, const char **ret,
			struct fb_part_par *f_part)
{
	struct fastboot_part_name *fs = f_part_names;
	struct fb_part_dev *fd = f_part->fd;
	struct fb_part_par *fp = f_part;
	const char *p, *id = parts;
	char str[16] = { 0, };
	int i;

	p = strchr(id, ':');
	if (!p) {
		printf("no <dev-id> identifier\n");
		return -EINVAL;
	}

	/* for next */
	p++, *ret = p;
	p--; parse_string(id, p, str, sizeof(str));

	for (i = 0; i < ARRAY_SIZE(f_part_names); i++, fs++) {
		if (!strcmp(fs->name, str)) {
			if (!(fd->part_support & fs->type)) {
				printf("** '%s' not support '%s' fs **\n",
				       fd->device, fs->name);
				return -EINVAL;
			}

			fp->type = fs->type;
			debug("fs    : %s\n", fs->name);
			return 0;
		}
	}

	printf("** Can't fs parse : %s **\n", str);
	return -EINVAL;
}

static int parse_seq_address(const char *parts, const char **ret,
			     struct fb_part_par *f_part)
{
	struct fb_part_par *fp = f_part;
	const char *p, *id = parts;
	char str[64] = { 0, };
	int id_len;

	p = strchr(id, ';');
	if (!p) {
		p = strchr(id, '\n');
		if (!p) {
			printf("no <; or '\n'> identifier\n");
			return -EINVAL;
		}
	}
	id_len = p - id;

	/* for next */
	p++, *ret = p;

	p = strchr(id, ',');
	if (!p) {
		printf("no <start> identifier\n");
		return -EINVAL;
	}

	parse_string(id, p, str, sizeof(str));
	fp->start = simple_strtoull(str, NULL, 16);
	debug("start : 0x%llx\n", fp->start);

	p++;
	parse_string(p, p + id_len, str, sizeof(str));	/* dev no*/
	fp->length = simple_strtoull(str, NULL, 16);
	debug("length: 0x%llx\n", fp->length);

	return 0;
}

static int parse_part_head(const char *parts, const char **ret)
{
	const char *p = parts;
	int len = strlen("flash=");

	debug("\n");
	p = strstr(p, "flash=");
	if (!p)
		return -EINVAL;

	*ret = p + len;
	return 0;
}

typedef int (parse_fnc_t)(const char *parts, const char **ret,
			   struct fb_part_par *f_part);

static parse_fnc_t *parse_part_seqs[] = {
	parse_seq_device,
	parse_seq_partition,
	parse_seq_fs,
	parse_seq_address,
	0,	/* end */
};

static void part_lists_init_all(void)
{
	struct list_head *fd_head = &f_dev_head;
	struct fb_part_dev *fd;
	struct fb_part_par *fp;
	struct list_head *entry, *n;

	list_for_each_entry(fd, fd_head, list) {
		struct list_head *head = &fd->part_list;

		if (!head->next)
			INIT_LIST_HEAD(head);

		if (!list_empty(head)) {
			debug("delete [%s]:", fd->device);
			list_for_each_safe(entry, n, head) {
				fp = list_entry(entry,
						struct fb_part_par, link);
				debug("%s ", fp->name);
				list_del(entry);
				free(fp);
			}
			debug("\n");
		}
		INIT_LIST_HEAD(head);
	}
}

static int part_lists_make(const char *ptable_str,
			   int ptable_str_len, bool save)
{
	struct fb_part_par *fp;
	parse_fnc_t **p_fnc_ptr;
	const char *p = ptable_str, *env;
	int len = ptable_str_len;
	int err = 0;

	debug("\n--- %s ---\n", __func__);

	part_lists_init_all();

	parse_comment(p, len);
	sort_string((char *)p, len);
	env = p;

	/* new parts table */
	while (1) {
		if (parse_part_head(p, &p)) {
			if (err)
				printf("-- unknown parts head: [%s]\n", p);
			/* EOF error */
			break;
		}

		fp = malloc(sizeof(*fp));
		if (!fp) {
			printf("* Can't malloc fastboot part table entry *\n");
			err = -EINVAL;
			break;
		}

		for (p_fnc_ptr = parse_part_seqs; *p_fnc_ptr; ++p_fnc_ptr) {
			if ((*p_fnc_ptr)(p, &p, fp) != 0) {
				free(fp);
				err = -EINVAL;
				goto fail_parse;
			}
		}
	}

fail_parse:
	if (!err && save) {
		env_set("partmap", env);
		saveenv();
	}

	if (err)
		part_lists_init_all();

	return err;
}

static void part_lists_print(void)
{
	struct list_head *fd_head = &f_dev_head;
	struct fb_part_dev *fd;
	struct fb_part_par *fp;
	int i;

	printf("\nFastboot Partitions:\n");
	list_for_each_entry(fd, fd_head, list) {
		list_for_each_entry(fp, &fd->part_list, link) {
			printf(" %s.%d: %s, %s : 0x%llx, 0x%llx\n",
			       fd->device, fp->dev_no, fp->name,
			       fp->type & PART_TYPE_PARTITION ?
					"partition" : "data",
			       fp->start, fp->length);
		}
	}

	printf("Support part type:");
	for (i = 0; i < ARRAY_SIZE(f_part_names); i++)
		printf("%s(%s) ", f_part_names[i].name,
		       f_part_names[i].type & PART_TYPE_PARTITION ?
		       "partition" : "data");
	printf("\n");
}

static int parse_env_head(const char *env, const char **ret, char *str, int len)
{
	const char *p = env, *r = p;

	parse_comment(p, len);
	r = strchr(p, '=');
	if (!r)
		return -EINVAL;

	if (parse_string(p, r, str, len) < 0)
		return -EINVAL;

	r = strchr(r, '"');
	if (!r) {
		printf("no <\"> identifier\n");
		return -EINVAL;
	}

	r++; *ret = r;
	return 0;
}

static int parse_env_end(const char *env, const char **ret, char *str, int len)
{
	const char *p = env;
	const char *r = p;

	r = strchr(p, '"');
	if (!r) {
		printf("no <\"> end identifier\n");
		return -EINVAL;
	}

	if (parse_string(p, r, str, len) < 0)
		return -EINVAL;

	r++;
	r = strchr(p, ';');
	if (!r) {
		r = strchr(p, '\n');
		if (!r) {
			printf("no <;> exit identifier\n");
			return -EINVAL;
		}
	}

	/* for next */
	r++, *ret = r;
	return 0;
}

static int parse_cmd(const char *cmd, const char **ret, char *str, int len)
{
	const char *p = cmd, *r = p;

	parse_comment(p, len);
	p = strchr(p, '"');
	if (!p)
		return -EINVAL;
	p++;

	r = strchr(p, '"');
	if (!r)
		return -EINVAL;

	if (parse_string(p, r, str, len) < 0)
		return -EINVAL;
	r++;

	r = strchr(p, ';');
	if (!r) {
		r = strchr(p, '\n');
		if (!r) {
			printf("no <;> exit identifier\n");
			return -EINVAL;
		}
	}

	/* for next */
	r++, *ret = r;
	return 0;
}

static int part_setenv(const char *str, int str_len)
{
	const char *p = str;
	int len = str_len;
	char cmd[128], arg[1024];
	int err = -1;

	debug("---partmap_setenv---\n");
	do {
		if (parse_env_head(p, &p, cmd, sizeof(cmd)))
			break;

		if (parse_env_end(p, &p, arg, sizeof(arg)))
			break;

		printf("%s=%s\n", cmd, arg);
		env_set(cmd, (char *)arg);
		saveenv();
		err = 0;
		len -= (int)(p - str);
	} while (len > 1);

	return err;
}

static int part_command(const char *str, int str_len)
{
	const char *p = str;
	int len = str_len;
	char cmd[128] = { 0, };
	int err = -1;

	debug("---partmap_command---\n");
	do {
		if (parse_cmd(p, &p, cmd, sizeof(cmd)))
			break;

		printf("Run [%s]\n", cmd);
		err = run_command(cmd, 0);
		if (err < 0)
			break;
		len -= (int)(p - str);
	} while (len > 1);

	return err;
}

static int part_dev_capacity(const char *device, u64 *length)
{
	struct list_head *fd_head = &f_dev_head;
	struct fb_part_dev *fd;
	const char *s = device, *c = device;
	char str[32] = {0,};
	u64 len = 0;
	int dev = 0;

	c = strchr(s, '.');
	if (c) {
		strncpy(str, s, (c - s));
		str[c - s] = 0;
		c += 1;
		dev = simple_strtoul(c, NULL, 10);

		list_for_each_entry(fd, fd_head, list) {
			if (strcmp(fd->device, str))
				continue;
			if (fd->ops && fd->ops->capacity)
				fd->ops->capacity(dev, &len);
			break;
		}
	}

	*length = len;

	return !len ? -EINVAL : 0;
}

static int part_table_update(void)
{
	struct list_head *fd_head = &f_dev_head;
	struct fb_part_dev *fd;
	struct fb_part_par *fp;
	int j, ret;

	debug("%s:\n", __func__);

	list_for_each_entry(fd, fd_head, list) {
		struct list_head *head = &fd->part_list;
		u64 part_dev[FASTBOOT_DEV_PART_MAX][3] = { {0, 0, 0}, };
		char *name_dev[FASTBOOT_DEV_PART_MAX];
		unsigned int part_table = PART_TYPE_PARTITION;
		int count = 0, dev = 0;
		int total = 0;

		if (list_empty(head))
			continue;

		list_for_each_entry(fp, head, link) {
			if (fp->type & PART_TYPE_PARTITION) {
				name_dev[total] = fp->name;
				part_dev[total][0] = fp->start;
				part_dev[total][1] = fp->length;
				part_dev[total][2] = fp->dev_no;
				part_table = fp->type;
				debug("%s.%d: %s: 0x%llx, 0x%llx [0x%x]\n",
				      fd->device, fp->dev_no, name_dev[total],
				      part_dev[total][0], part_dev[total][1],
				      fp->type);
				total++;
			}
		}
		debug("total parts : %d\n", total);

		count = total;
		while (count > 0) {
			u64 parts[FASTBOOT_DEV_PART_MAX][2] = { {0, 0 }, };
			char *names[FASTBOOT_DEV_PART_MAX] = { 0, };
			int num = 0;

			if (fd->dev_max < dev) {
				printf("** Fail make to %s dev %d",
				       fd->device, dev);
				printf("is over max %d **\n", fd->dev_max);
				break;
			}

			for (j = 0; j < total; j++) {
				if (dev == (int)part_dev[j][2]) {
					names[num] = name_dev[j];
					parts[num][0] = part_dev[j][0];
					parts[num][1] = part_dev[j][1];
					debug("Partition %s.%d 0x%llx,",
					      fd->device, dev, parts[num][0]);
					debug("0x%llx (%d:%d)\n",
					      parts[num][1], total, count);
					num++;
				}
			}

			/* new partition */
			if (num && fd->ops && fd->ops->create_part) {
				ret = fd->ops->create_part(dev,
							   names, parts, num,
							   part_table);
				if (ret)
					return ret;
			}

			count -= num;
			debug("count %d, tables %d, dev %d\n", count, num, dev);
			if (count)
				dev++;
		}
	}

	return 0;
}

static struct fb_part_par *part_lookup(const char *cmd)
{
	struct list_head *fd_head = &f_dev_head;
	struct fb_part_dev *fd;
	struct fb_part_par *fp;

	list_for_each_entry(fd, fd_head, list) {
		list_for_each_entry(fp, &fd->part_list, link) {
			if (strcmp(fp->name, cmd))
				continue;
			return fp;
		}
	}

	return NULL;
}

static int part_write(const char *cmd, void *download_buffer,
		      unsigned int download_bytes,
		      char *response)
{
	struct fb_part_dev *fd;
	struct fb_part_par *fp;

	fp = part_lookup(cmd);
	if (!fp)
		return -EINVAL;

	/*
	 * skip check download_bytes for sparse images
	 */

	fd = fp->fd;
	if (fd && fd->ops && fd->ops->write) {
		int err = fd->ops->write(fp, download_buffer, download_bytes);

		if (err < 0) {
			sprintf(response, "FAILFlash write");
			return -EFAULT;
		}
	}

	return 0;
}

static int strcmp_l1(const char *s1, const char *s2)
{
	if (!s1 || !s2)
		return -EINVAL;

	return strncmp(s1, s2, strlen(s1));
}

int cb_getvar_ext(char *cmd, char *response, size_t chars_left)
{
	char *s = cmd;
	int err;

	if (!strcmp_l1("partition-type:", cmd)) {
		s += strlen("partition-type:");
		sprintf(s, "Ready");
		strncat(response, s, chars_left);
		return 0;
	} else if (!strcmp_l1("product", cmd)) {
		strncat(response, CONFIG_SYS_BOARD, chars_left);
		return 0;
	} else if (!strcmp_l1("capacity", cmd)) {
		u64 length;

		s += strlen("capacity");
		s += 1;
		err = part_dev_capacity(s, &length);
		if (err < 0) {
			printf("unknown device: %s\n", s);
			strcpy(s, "FAILUnknown device");
			strncat(response, s, chars_left);
			return -ENODEV;
		}

		sprintf(s, "%lld", length);
		strncat(response, s, chars_left);
		return 0;
	} else if (!strcmp_l1("0x", cmd)) {
		u32 addr, data;

		addr = simple_strtoul(s, NULL, 16);
		data = readl(addr);

		sprintf(s, "%x", data);
		strncat(response, s, chars_left);
		debug("reg: 0x%x : 0x%x\n", addr, data);
		return 0;
	}

	return -EINVAL;
}

int cb_flash_ext(char *cmd, char *response, unsigned int download_bytes)
{
	char *p = (void *)CONFIG_FASTBOOT_BUF_ADDR;
	int ret;

	if (!strcmp("partmap", cmd)) {
		char tmp[2048];

		if (sizeof(tmp) < download_bytes) {
			printf("partmap size buffer %u overflow %u\n",
			       download_bytes, sizeof(tmp));
			sprintf(response, "FAILPartmap %u Overflow %u",
				download_bytes, sizeof(tmp));
			return -ENOMEM;
		}

		strncpy(tmp, p, download_bytes);
		if (part_lists_make(tmp, download_bytes, true) < 0) {
			sprintf(response, "FAILMake Partition Table");
			return -ENOMEM;
		}

		part_lists_print();

		ret = part_table_update();
		if (ret)
			return ret;

		fastboot_okay(response);
		return 0;
	} else if (!strcmp("setenv", cmd)) {
		if (part_setenv(p, download_bytes) < 0) {
			sprintf(response, "FAILEnv write");
			return -EFAULT;
		}
		fastboot_okay(response);
		return 0;
	} else if (!strcmp("cmd", cmd)) {
		if (part_command(p, download_bytes) < 0) {
			sprintf(response, "FAILRun command");
			return -EFAULT;
		}
		fastboot_okay(response);
		return 0;
	}

	return part_write(cmd, p, download_bytes, response);
}

void __weak fb_partmap_add_dev_mmc(struct list_head *head)
{
}

void __weak fb_partmap_add_dev_spi(struct list_head *head)
{
}

void __weak fb_partmap_add_dev_nand(struct list_head *head)
{
}

void fastboot_bind_ext(void)
{
	char *env;

	debug("%s : %s\n", __func__, f_dev_binded ? "binded" : "bind");

	if (f_dev_binded)
		return;

	f_dev_binded = true;

	fb_partmap_add_dev_mmc(&f_dev_head);
	fb_partmap_add_dev_spi(&f_dev_head);
	fb_partmap_add_dev_nand(&f_dev_head);
	part_lists_init_all();

	env = env_get("partmap");
	if (env)
		part_lists_make(env, strlen(env), false);
}

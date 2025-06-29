/*
  Based on CUSE example: Character device in Userspace
  Copyright (C) 2008-2009  SUSE Linux Products GmbH
  Copyright (C) 2008-2009  Tejun Heo <tj@kernel.org>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall drmd.c `pkg-config fuse --cflags --libs` -o drmd
*/

#define FUSE_USE_VERSION 29
#define DRM_IOCTL_SET_MASTER            0x1e

#include <cuse_lowlevel.h>
#include <fuse_opt.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/ioctl.h>

enum {
        FIOC_GET_SIZE   = _IOR('E', 0, size_t),
        FIOC_SET_SIZE   = _IOW('E', 1, size_t),

        /*
         * The following two ioctls don't follow usual encoding rules
         * and transfer variable amount of data.
         */
        FIOC_READ       = _IO('E', 2),
        FIOC_WRITE      = _IO('E', 3),
};

struct fioc_rw_arg {
        off_t           offset;
        void            *buf;
        size_t          size;
        size_t          prev_size;      /* out param for previous total size */
        size_t          new_size;       /* out param for new total size */
};

static void *drmd_buf;
static size_t drmd_size;

static const char *usage =
"usage: drmd [options]\n"
"\n"
"options:\n"
"    --help|-h             print this help message\n"
"    --maj=MAJ|-M MAJ      device major number\n"
"    --min=MIN|-m MIN      device minor number\n"
"    --name=NAME|-n NAME   device name (mandatory)\n"
"\n";

static int drmd_resize(size_t new_size)
{
	void *new_buf;

	if (new_size == drmd_size)
		return 0;

	new_buf = realloc(drmd_buf, new_size);
	if (!new_buf && new_size)
		return -ENOMEM;

	if (new_size > drmd_size)
		memset(new_buf + drmd_size, 0, new_size - drmd_size);

	drmd_buf = new_buf;
	drmd_size = new_size;

	return 0;
}

static int drmd_expand(size_t new_size)
{
	if (new_size > drmd_size)
		return drmd_resize(new_size);
	return 0;
}

static void drmd_open(fuse_req_t req, struct fuse_file_info *fi)
{
	fuse_reply_open(req, fi);
}

static void drmd_read(fuse_req_t req, size_t size, off_t off,
			 struct fuse_file_info *fi)
{
	(void)fi;

	if (off >= drmd_size)
		off = drmd_size;
	if (size > drmd_size - off)
		size = drmd_size - off;

	fuse_reply_buf(req, drmd_buf + off, size);
}

static void drmd_write(fuse_req_t req, const char *buf, size_t size,
			  off_t off, struct fuse_file_info *fi)
{
	(void)fi;

	if (drmd_expand(off + size)) {
		fuse_reply_err(req, ENOMEM);
		return;
	}

	memcpy(drmd_buf + off, buf, size);
	fuse_reply_write(req, size);
}

static void fioc_do_rw(fuse_req_t req, void *addr, const void *in_buf,
		       size_t in_bufsz, size_t out_bufsz, int is_read)
{
	const struct fioc_rw_arg *arg;
	struct iovec in_iov[2], out_iov[3], iov[3];
	size_t cur_size;

	/* read in arg */
	in_iov[0].iov_base = addr;
	in_iov[0].iov_len = sizeof(*arg);
	if (!in_bufsz) {
		fuse_reply_ioctl_retry(req, in_iov, 1, NULL, 0);
		return;
	}
	arg = in_buf;
	in_buf += sizeof(*arg);
	in_bufsz -= sizeof(*arg);

	/* prepare size outputs */
	out_iov[0].iov_base =
		addr + (unsigned long)&(((struct fioc_rw_arg *)0)->prev_size);
	out_iov[0].iov_len = sizeof(arg->prev_size);

	out_iov[1].iov_base =
		addr + (unsigned long)&(((struct fioc_rw_arg *)0)->new_size);
	out_iov[1].iov_len = sizeof(arg->new_size);

	/* prepare client buf */
	if (is_read) {
		out_iov[2].iov_base = arg->buf;
		out_iov[2].iov_len = arg->size;
		if (!out_bufsz) {
			fuse_reply_ioctl_retry(req, in_iov, 1, out_iov, 3);
			return;
		}
	} else {
		in_iov[1].iov_base = arg->buf;
		in_iov[1].iov_len = arg->size;
		if (arg->size && !in_bufsz) {
			fuse_reply_ioctl_retry(req, in_iov, 2, out_iov, 2);
			return;
		}
	}

	/* we're all set */
	cur_size = drmd_size;
	iov[0].iov_base = &cur_size;
	iov[0].iov_len = sizeof(cur_size);

	iov[1].iov_base = &drmd_size;
	iov[1].iov_len = sizeof(drmd_size);

	if (is_read) {
		size_t off = arg->offset;
		size_t size = arg->size;

		if (off >= drmd_size)
			off = drmd_size;
		if (size > drmd_size - off)
			size = drmd_size - off;

		iov[2].iov_base = drmd_buf + off;
		iov[2].iov_len = size;
		fuse_reply_ioctl_iov(req, size, iov, 3);
	} else {
		if (drmd_expand(arg->offset + in_bufsz)) {
			fuse_reply_err(req, ENOMEM);
			return;
		}

		memcpy(drmd_buf + arg->offset, in_buf, in_bufsz);
		fuse_reply_ioctl_iov(req, in_bufsz, iov, 2);
	}
}

static void drmd_ioctl(fuse_req_t req, int cmd, void *arg,
			  struct fuse_file_info *fi, unsigned flags,
			  const void *in_buf, size_t in_bufsz, size_t out_bufsz)
{
	int is_read = 0;

	(void)fi;

	if (flags & FUSE_IOCTL_COMPAT) {
		fuse_reply_err(req, ENOSYS);
		return;
	}
    printf("cmd=%d\n",cmd);
	switch (cmd) {
	case FIOC_GET_SIZE:
		if (!out_bufsz) {
			struct iovec iov = { arg, sizeof(size_t) };

			fuse_reply_ioctl_retry(req, NULL, 0, &iov, 1);
		} else
			fuse_reply_ioctl(req, 0, &drmd_size,
					 sizeof(drmd_size));
		break;

	case FIOC_SET_SIZE:
		if (!in_bufsz) {
			struct iovec iov = { arg, sizeof(size_t) };

			fuse_reply_ioctl_retry(req, &iov, 1, NULL, 0);
		} else {
			drmd_resize(*(size_t *)in_buf);
			fuse_reply_ioctl(req, 0, NULL, 0);
		}
		break;

	case FIOC_READ:
		is_read = 1;
	case FIOC_WRITE:
		fioc_do_rw(req, arg, in_buf, in_bufsz, out_bufsz, is_read);
		break;

    case 25630: //DRM_IOCTL_SET_MASTER
        fuse_reply_ioctl(req, 0, NULL, 0);
        break;

    case 25631: //DRM_IOCTL_DROP_MASTER
        fuse_reply_ioctl(req, 0, NULL, 0);
        break;


	default:
		fuse_reply_err(req, EINVAL);
	}
}

struct drmd_param {
	unsigned		major;
	unsigned		minor;
	char			*dev_name;
	int			is_help;
};

#define DRMD_OPT(t, p) { t, offsetof(struct drmd_param, p), 1 }

static const struct fuse_opt drmd_opts[] = {
	DRMD_OPT("-M %u",		major),
	DRMD_OPT("--maj=%u",		major),
	DRMD_OPT("-m %u",		minor),
	DRMD_OPT("--min=%u",		minor),
	DRMD_OPT("-n %s",		dev_name),
	DRMD_OPT("--name=%s",	dev_name),
	FUSE_OPT_KEY("-h",		0),
	FUSE_OPT_KEY("--help",		0),
	FUSE_OPT_END
};

static int drmd_process_arg(void *data, const char *arg, int key,
			       struct fuse_args *outargs)
{
	struct drmd_param *param = data;

	(void)outargs;
	(void)arg;

	switch (key) {
	case 0:
		param->is_help = 1;
		fprintf(stderr, "%s", usage);
		return fuse_opt_add_arg(outargs, "-ho");
	default:
		return 1;
	}
}

static const struct cuse_lowlevel_ops drmd_clop = {
	.open		= drmd_open,
	.read		= drmd_read,
	.write		= drmd_write,
	.ioctl		= drmd_ioctl,
};n

int main(int argc, char **argv)
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct drmd_param param = { 0, 0, NULL, 0 };
	char dev_name[128] = "DEVNAME=";
	const char *dev_info_argv[] = { dev_name };
	struct cuse_info ci;

	if (fuse_opt_parse(&args, &param, drmd_opts, drmd_process_arg)) {
		printf("failed to parse option\n");
		return 1;
	}

	if (!param.is_help) {
		if (!param.dev_name) {
			fprintf(stderr, "Error: device name missing\n");
			return 1;
		}
		strncat(dev_name, param.dev_name, sizeof(dev_name) - 9);
	}

	memset(&ci, 0, sizeof(ci));
	ci.dev_major = param.major;
	ci.dev_minor = param.minor;
	ci.dev_info_argc = 1;
	ci.dev_info_argv = dev_info_argv;
	ci.flags = CUSE_UNRESTRICTED_IOCTL;

	return cuse_lowlevel_main(args.argc, args.argv, &ci, &drmd_clop,
				  NULL);
}

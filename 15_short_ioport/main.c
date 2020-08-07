#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <asm/io.h>

#include "main.h"

static int major = 0;
static int short_irq = 6;

static unsigned long base = 0x200;
unsigned long short_base = 0;

int short_open(struct inode *inode, struct file *filp)
{
	return 0;
}

int short_release(struct inode *inode, struct file *filp)
{
	return 0;
}

ssize_t short_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	unsigned long port = short_base;
	unsigned char *kbuf;
	int i;

	kbuf = kzalloc(count, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	i = *f_pos;

	while (i < count) {
		kbuf[i] = inb(port);
		rmb();
		if (kbuf[i++] == '\n')
			break;
	}

	if (copy_to_user(buf, kbuf, i)) {
		i = -EFAULT;
	}

	kfree(kbuf);

	return i;
}

ssize_t short_write(struct file *filp, const char __user *buf, size_t count,
		loff_t *f_pos)
{
	unsigned long port = short_base;
	unsigned char *kbuf, *ptr;
	size_t cnt = count;

	kbuf = kmalloc(cnt, GFP_KERNEL);

	if (!kbuf)
		return -ENOMEM;
	if (copy_from_user(kbuf, buf, cnt))
		return -EFAULT;

	ptr = kbuf;

	while (cnt--) {
		pr_debug("port=%lu(%#lx), val=%d(%#x)\n", port, port, *ptr, *ptr);
		outb(*(ptr++), port);
		wmb();
	}

	kfree(kbuf);

	return count;
}

static struct file_operations fops = {
	.owner	 = THIS_MODULE,
	.read	 = short_read,
	.write	 = short_write,
	// .poll	 = short_poll,
	.open	 = short_open,
	.release = short_release,
};

irqreturn_t irq_service(int irq, void *dev_id)
{
	if (short_irq != irq)
		short_irq = -irq;

	pr_debug("IRQ -----\n");

	return IRQ_HANDLED;
}

static
int __init m_init(void)
{
	int result = 0;
	short_base = base;

	pr_err("Hello!\n");
	pr_err("base = %#lx\n", short_base);
	if (!request_region(short_base, SHORT_NR_PORTS, "short")) {
		PDEBUG("short: cannot get I/O port address %#lx\n", short_base);
		result = -ENODEV;
		goto out;
	}

	if (short_irq >= 0) {
		result = request_irq(short_irq, irq_service, 0,
				     MODULE_NAME, NULL);
		if (result) {
			pr_err("Request irq %d failed\n", short_irq);
			short_irq = -1;
			result = -ENODEV;
			goto unreg_region;
		}
	}

	result = register_chrdev(major, MODULE_NAME, &fops);
	if (result < 0) {
		pr_err("cannot get major number!\n");
		goto unreg_irq;
	}
	major = (major == 0) ? result : major;

	return 0;

unreg_irq:
	free_irq(short_irq, NULL);

unreg_region:
	release_region(short_base, SHORT_NR_PORTS);

out:
	return result;
}

static
void __exit m_exit(void)
{
	unregister_chrdev(major, MODULE_NAME);
	free_irq(short_irq, NULL);
	release_region(short_base, SHORT_NR_PORTS);
}


module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Basic io port manipulation based on Parallel Port");

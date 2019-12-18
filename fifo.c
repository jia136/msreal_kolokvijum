#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
#include <linux/sched.h>

#define BUFF_SIZE 100
#define MAX 16
MODULE_LICENSE("Dual BSD/GPL");

dev_t my_dev_id;
static struct class *my_class;
static struct device *my_device;
static struct cdev *my_cdev;


DECLARE_WAIT_QUEUE_HEAD(readQ);
DECLARE_WAIT_QUEUE_HEAD(writeQ);
struct semaphore sem;

unsigned char fifo[MAX];
int in = 0;
int out = 0;
int brojac = 0;
int endRead = 0;
bool mod = false;
char fifo_mat[MAX][5];
int n = 1;


int fifo_open(struct inode *pinode, struct file *pfile);
int fifo_close(struct inode *pinode, struct file *pfile);
ssize_t fifo_read(struct file *pfile, char __user *buffer, size_t length, loff_t *offset);
ssize_t fifo_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset);

struct file_operations my_fops =
{
	.owner = THIS_MODULE,
	.open = fifo_open,
	.read = fifo_read,
	.write = fifo_write,
	.release = fifo_close,
};


int fifo_open(struct inode *pinode, struct file *pfile) 
{
		printk(KERN_INFO "Succesfully opened fifo\n");
		return 0;
}

int fifo_close(struct inode *pinode, struct file *pfile) 
{
		printk(KERN_INFO "Succesfully closed fifo\n");
		return 0;
}

ssize_t fifo_read(struct file *pfile, char __user *buffer, size_t length, loff_t *offset)
{
	int ret;
	char buff[BUFF_SIZE];
	long int len = 0;

	if (endRead == n){
		endRead = 0;
		return 0;
	}

	if(down_interruptible(&sem))
		return -ERESTARTSYS;
	while(brojac == 0)
	{
		up(&sem);
		if(wait_event_interruptible(readQ,(brojac>0)))
			return -ERESTARTSYS;
		if(down_interruptible(&sem))
			return -ERESTARTSYS;
	}

	if(brojac != 0)
	{
		if (endRead == n){
			endRead = 0;
      			up(&sem);
			wake_up_interruptible(&writeQ);
			return 0;
		}
		if(mod)
			len = scnprintf(buff, BUFF_SIZE, "%d ",fifo[out]);
		else
			len = scnprintf(buff, BUFF_SIZE, "0x%s ", fifo_mat[out]);

		ret = copy_to_user(buffer, buff, len);
		if(ret)
			return -EFAULT;

		printk(KERN_INFO "Succesfully read\n");
		printk(KERN_INFO "endread=%d\n", endRead);
		out = (out+1)%(MAX);
		brojac--;
		endRead++;
	}

	else
	{
			printk(KERN_WARNING "fifo is empty\n");
	}

 	up(&sem);
	wake_up_interruptible(&writeQ);
	
	return len;
}

ssize_t fifo_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset)
{
	char buff[BUFF_SIZE];
	char value[50];
	int ret;
	int duzina = 0;
	int j = 0;
        int i =0;
	char temp [20];
	int vrednost = 0;
	int pom = 0;
	int k = 0;
	char ns[3];
	char dest[6];
	int c = 0;

	for(c = 0; c < 6; c++)
		dest[c] = '\0';
	ret = copy_from_user(buff, buffer, length);
	if(ret)
		return -EFAULT;
	buff[length-1] = '\0';
	
	ret = sscanf(buff,"%s",value);
	duzina = strlen(value);
	strncpy(dest,value,4);

	if( ret == 1 ){
		if(strcmp(value, "hex")==0){
			mod = false;
		}
		else if(strcmp(value, "dec")==0){
			mod = true;
		}
		else if(strcmp(dest,"num=")==0){
			printk(KERN_INFO "DEST = %s ", dest);
			if(value[5]=='\0'){
				ns[0] = value[4];
				ns[1] = '\0';
			}
			else{
				ns[0] = value[4];
				ns[1] = value[5];
				ns[2] ='\0';
			}
			pom = kstrtoint(ns,10,&n);
			if(n > 16)
				n = 1;
			if(n < 1)
				n = 1;
			printk(KERN_INFO "n = %d", n);
			endRead = 0;
		}
		else{
			if(down_interruptible(&sem))
				return -ERESTARTSYS;
			while(brojac == MAX)
			{
				up(&sem);
				if(wait_event_interruptible(writeQ,(brojac<MAX)))
					return -ERESTARTSYS;
				if(down_interruptible(&sem))
					return -ERESTARTSYS;
			}

			 if(brojac != MAX){
				for(i = 0; i<=duzina; i++)
				{
					if(value[i] != ',' && value[i] != '\0'){
						temp[j] = value[i];
						j++;
					}
					else{
						temp[j] = '\0';
						if(j > 2){
							printk(KERN_INFO "Los unos!");
							up(&sem);
							wake_up_interruptible(&readQ);
							return length;
						}
					j = 0;

					pom = kstrtoint(temp,16,&vrednost);
					fifo[in] = vrednost;
					if(vrednost > 255){
						printk(KERN_INFO "los unos!");
						return length;
					}

					for(k = 0; k <strlen(temp); k++){
						fifo_mat[in][k] = temp[k];
					}
					fifo_mat[in][k] = '\0';

					if(!mod)
						printk(KERN_INFO "Succedfully wrote string %s ", fifo_mat[in]);
					else
						printk(KERN_INFO "Succedfully wrote value %d ", vrednost);

					in=(in+1)%(MAX);
					brojac++;

					while(brojac == MAX)
					{
						up(&sem);
						if(wait_event_interruptible(writeQ,(brojac<MAX)))
							return -ERESTARTSYS;
						if(down_interruptible(&sem))
							return -ERESTARTSYS;
					}
					
					}
				}
			}
		}
	}
	else
	{
		printk(KERN_WARNING "Wrong command format\n");
	}

	up(&sem);
	wake_up_interruptible(&readQ);

	return length;
}

static int __init fifo_init(void)
{
   	int ret = 0;
	int i=0;
	int j = 0;
	sema_init(&sem,1);
	//Initialize array
	for (i=0; i<MAX; i++)
		fifo[i] = 0;
	for(i = 0; i< MAX; i++){
		for(j = 0; j < 5; j++)
			fifo_mat[i][j] ='\0';
	}

   	ret = alloc_chrdev_region(&my_dev_id, 0, 1, "fifo");
  	if (ret){
		printk(KERN_ERR "failed to register char device\n");
      		return ret;
   	}
  	 printk(KERN_INFO "char device region allocated\n");

   	my_class = class_create(THIS_MODULE, "fifo_class");
   	if (my_class == NULL){
      		printk(KERN_ERR "failed to create class\n");
      		goto fail_0;
   	}
   	printk(KERN_INFO "class created\n"); 
   	my_device = device_create(my_class, NULL, my_dev_id, NULL, "fifo");
  	if (my_device == NULL){
      		printk(KERN_ERR "failed to create device\n");
      		goto fail_1;
   	}
   	printk(KERN_INFO "device created\n");
	my_cdev = cdev_alloc();	
	my_cdev->ops = &my_fops;
	my_cdev->owner = THIS_MODULE;
	ret = cdev_add(my_cdev, my_dev_id, 1);
	if (ret)
	{
      		printk(KERN_ERR "failed to add cdev\n");
		goto fail_2;
	}
  	 printk(KERN_INFO "cdev added\n");
   	printk(KERN_INFO "Hello world\n");

   	return 0;

   	fail_2:
      		device_destroy(my_class, my_dev_id);
  	fail_1:
      		class_destroy(my_class);
  	fail_0:
      		unregister_chrdev_region(my_dev_id, 1);
  	 return -1;
}

static void __exit fifo_exit(void)
{
	cdev_del(my_cdev);
 	device_destroy(my_class, my_dev_id);
  	class_destroy(my_class);
  	unregister_chrdev_region(my_dev_id,1);
   	printk(KERN_INFO "Goodbye, cruel world\n");
}


module_init(fifo_init);
module_exit(fifo_exit);
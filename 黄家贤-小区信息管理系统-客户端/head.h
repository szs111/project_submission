/*
	头文件
*/
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>			//线程信号
#include <stdlib.h>
#include <unistd.h>			//进程
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <strings.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/msg.h>		//消息队列
#include <sys/ipc.h>		//消息队列
#include <sys/shm.h>		//共享内存
#include <linux/fb.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <semaphore.h>		//信号量
#include <arpa/inet.h>		//切换ip地址
#include <sys/socket.h>		//TCP创建对象
#include <netinet/in.h>		//IPV4
#include <netinet/ip.h>		//IPV4
#include <sys/select.h>		//多路复用
#include <linux/input.h>




//创建字库
//				字					字体大小	画板大小x		画板大小y	字体框x		字体框y
int font_(char font_buf[1024], int font_size, int bm_size_x, int bm_size_y, int font_x, int font_y);
int bmp_show(char *bmp_name,int bmp_length,int bmp_height,int bmp_x,int bmp_y);
int init_lcd();		//初始化开发板
int tcp();			//TCP通信协议
int get_ts();		//获取触摸屏坐标
extern struct user * head;

//设计结点
struct user
{
	char name[200];
	struct user *next;
};

bool empty(struct user * head)
{
	return head == NULL;
}

//新建一个结点
struct user * new_user(char *name)
{
	//为新结点分配空间
	struct user *new = malloc(sizeof(struct user)); 
	//初始化新节点
	if(new != NULL)
	{
		strcpy(new->name,name);
		new->next = NULL;
	}
	return new;
}

//将新节点new，插入到链表的末尾，称为新的尾节点
void add_list_tail( struct user *new)
{
	// 定义一个尾指针
	struct user * tail;

	// 让tail从头开始往后遍历，直到找到末尾节点为止
	tail = head;
	while(tail->next != head)
		tail = tail->next;

	// 让原链表的末尾节点，指向新的尾节点
	tail->next = new;

	// 将新的尾节点，指向首节点
	new->next = head;
	
}

// 输出整条链表的节点
void show(struct user * head)
{
	if(empty(head))
	{
		printf("链表是空的\n");
		return;
	}
	int a=135;
	struct user * p = head->next;
	do{
		font_(p->name, 30, 200, 31 , 500, a);
		printf("%s\n", p->name);
		p = p->next;
		a+=35;
	}while(p != head);
}

//删除结点
void del(char *name)
{
	struct user *pos=head->next;
	struct user *ptr=head;
	
	for(pos=head->next;pos!=head;ptr=pos,pos=pos->next)
	{
		if(strcmp(pos->name,name)==0)
		{
			ptr->next=pos->next;
			pos->next=NULL;
			free(pos);
			break;
		}
	}
}

#include<stdio.h>
#include <sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>         
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include<string.h>
#include<unistd.h>
#include<pthread.h>
#include<stdlib.h>
#include<netdb.h>

#define MY_IP "192.168.22.206"//服务器IP地址
#define MAX_THREAD_NUM 20//最大线程数
#define MAX_TASK_NUM  1000//最大任务数量

//设计任务结构体
struct task
{
    void *(*do_task)(void *arg);//任务函数
    void *arg;//任务函数的参数
    struct task *next;
};

//设计一个线程池管理结构体
struct thread_pool
{
    pthread_mutex_t lock;//线程锁
    pthread_cond_t cond;//条件变量
    
    int poweron;//线程池运行状态
    
    struct task *head;//任务链表头结点
    pthread_t *TID;//保存tid号
    
    unsigned int total_thread_num;//线程总数量
    unsigned int total_task_num;//任务总数量
    unsigned int waiting_num;//正在等待任务数量
};

//用户信息结构体
struct people
{
    char name[200];//用户名字
    char ip[200];//用户的ip地址
    int port;//对方的端口
    int socket;//服务器中对方的socket
    struct people *next;
    struct people *prev;
};

struct people *head=NULL;

/*
*功能说明:添加任务函数
*参数1：指向线程池的指针
*参数2：传递给任务函数的参数
*返回值：成功返回 0 ；  失败返回 -1
*/
int add_task(struct thread_pool *pool,void *(do_task)(void *arg),void *arg)
{
    //创建一个任务结构体
    struct task *new_task=malloc(sizeof(struct task));
     if(new_task==NULL)
    {
        printf("添加任务失败\n");
        return -1;
    }
    new_task->do_task=do_task;
    new_task->arg=arg;
    new_task->next=NULL;
    
    //上锁
    pthread_mutex_lock(&pool->lock);
    
    //任务等待数量>=任务总数量
    if(pool->waiting_num>=pool->total_task_num)
    {
        //解锁并释放堆空间
        pthread_mutex_unlock(&pool->lock);
        free(new_task);
        printf("添加任务失败\n");
        return -1;
    }
    
    //添加到任务链表
    struct task *pos=pool->head;
    while(pos->next!=NULL)
    {
        pos=pos->next;
    }
    pos->next=new_task;    
    pool->waiting_num++;
    
    //解锁
    pthread_mutex_unlock(&pool->lock);
    
    //唤醒线程执行任务
    pthread_cond_signal(&pool->cond);
    return 0;
}

/*
*功能说明:当接到线程取消信号解锁  
*参数：void型指针
*/
void handler(void *arg)
{
    pthread_mutex_unlock((pthread_mutex_t *)arg);//解锁
}

/*
*功能说明:将线程睡眠，等待执行任务   
*参数：void型指针
*/
void *route(void *arg)
{
    struct thread_pool *pool=arg;
    struct task *pos=NULL;
    while(1)
    {
        //注册取消信号屏蔽函数
        pthread_cleanup_push(handler, (void *)&pool->lock);
        //上锁
        pthread_mutex_lock(&pool->lock);
        while(pool->waiting_num==0 && pool->poweron==1)//没有等待任务并且在开机状态下等待唤醒
        {
            pthread_cond_wait(&pool->cond,&pool->lock);
        }
        if(pool->waiting_num==0 && pool->poweron==0)//没有等待任务并且在关机状态下解锁并退出线程
        {
            pthread_mutex_unlock(&pool->lock);
            pthread_exit(NULL);
        }
        //取出任务
        pos=pool->head->next;
        pool->head->next=pos->next;
        pos->next=NULL;
        pool->waiting_num--;
        
        //解锁
        pthread_mutex_unlock(&pool->lock);
        //关闭注册取消信号屏蔽函数
        pthread_cleanup_pop(0);
        
        //屏蔽取消信号
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        //执行任务函数
        (pos->do_task)(pos->arg);
        //解除屏蔽
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        free(pos);
    }
}

/*
*功能说明:初始化线程池    
*参数1：指向要初始化的线程池指针
*参数2：需要创建的线程总数
*返回值：成功返回 0 ；  失败返回 -1
*/
int init_thread_pool(struct thread_pool *pool,unsigned int toal_thread_num)
{
    if(toal_thread_num>MAX_THREAD_NUM)
    {
        printf("超过能创建的线程总数，初始化线程池失败!\n");
        return -1;
    }
    //初始化线程锁
    pthread_mutex_init(&pool->lock,NULL);
    //初始化条件变量
    pthread_cond_init(&pool->cond,NULL);
    //设置线程池状态
    pool->poweron=1;
    //创建任务链表的头结点
    pool->head=malloc(sizeof(struct task));
    //为TID分配堆空间
    pool->TID=malloc(sizeof(pthread_t)*MAX_THREAD_NUM);
    if(pool->head==NULL||pool->TID==NULL)
    {
        printf("堆空间分配失败，初始化线程池失败!\n");
        return -1;
    }
    //将任务链表的头结点里的next置为空
    pool->head->next=NULL;
    //线程总数量
    pool->total_thread_num=toal_thread_num;
    //任务总数量
    pool->total_task_num=MAX_TASK_NUM;
    //正在等待任务的数量
    pool->waiting_num=0;
    
    //创建线程
    int i=0;
    for(i=0;i<pool->total_thread_num;i++)
    {
        if(pthread_create(&(pool->TID[i]),NULL,route,(void *)pool)!=0)
        {
            printf("线程创建失败，初始化线程池失败!\n");
            return-1;
        }
    }
    return 0;
}

/*
*功能说明:销毁线程池   
*参数1：指向要初始化的线程池指针
*返回值：成功返回 0 
*/
int destroy_pool(struct thread_pool *pool)
{
    //关机
    pool->poweron = 0;
    //唤醒所有休眠的线程
    pthread_cond_broadcast(&pool->cond);

    //回收线程资源
    int i;
    for(i=0; i < pool->total_thread_num; i++)
    {
        pthread_join(pool->TID[i], NULL);
    }

    //释放堆空间
    free(pool->head);
    free(pool->TID);
    free(pool);
    return 0;
}

/*
*显示链表
*/
void show_list()
{
    struct people *pos=head->next;
    for(pos=head->next;pos!=head;pos=pos->next)
    {
        printf("名字: %s\n",pos->name);
    }
}

/*
*服务器工作区
*/
void *server_func(void *arg)
{
    while(1)
    {
        printf("请选择功能 1.发送公告 2.显示当前在线客户列表 3.跑路了\n");
        int a=0;
        scanf("%d",&a);
        while(getchar()!='\n');//清空缓存区
        if(a==1)//发送公告
        {
            printf("请输入公告内容\n");
            char buf[1024]={0};
            scanf("%s",buf);
            //拼接
            char send_buf[2000]={0};
            sprintf(send_buf,"&*&*:%s",buf);
            struct people *pos=head->next;
            for(pos=head->next;pos!=head;pos=pos->next)
            {
                write(pos->socket,send_buf,strlen(send_buf));
            }   
        }
        else if(a==2)//显示当前在线客户列表
        {
            show_list();
        }
        else if(a==3)
        {
            exit(0);//让退出进程函数去释放资源，嘿嘿
        }
        else
        {
            printf("输入错误,请重新选择\n");
        }
    }
}

/*
*将用户信息添加进链表
*/
void add_into_list(char *ip,int port,int socket)
{
    struct people *new=malloc(sizeof(struct people));
    strcpy(new->ip,ip);
    new->port=port;
    new->socket=socket;
    //链接
    new->prev=head->prev;
    new->next=head;
    head->prev->next=new;
    head->prev=new;
}

/*
*删除链表中的信息
*/
void del_list(char *name)
{
    struct people *pos=head->next;
    for(pos=head->next;pos!=head;pos=pos->next)
    {
        if(strcmp(pos->name,name)==0)
        {
            struct people *ptr=pos->prev;
            ptr->next=pos->next;
            pos->next->prev=ptr;
            pos->prev=NULL;
            pos->next=NULL;
            free(pos);
            return;
        }
    }
}

//文件的配套切割
void file_tok(char *buf,char *name,char *file_name,int *file_len,char *new_buf)
{
    char *p=strstr(buf,"****");
    char tmp[1024]={0};
    strcpy(tmp,p+4);
    p=strstr(tmp,"****");
    strncpy(name,tmp,(int)(p-tmp));
    strcpy(tmp,p+4);
    p=strstr(tmp,"****");
    strncpy(file_name,tmp,(int)(p-tmp));
    strcpy(tmp,p+4);
    p=strstr(tmp,"****");
    char len[100]={0};
    strncpy(len,tmp,(int)(p-tmp));
    sscanf(len,"%d",file_len);
    strcpy(new_buf,p+4);
}

//发送消息配套切割
void send_tok(char *buf,char *name,char *new_buf)
{
    char *p=strstr(buf,"^^:");
    char *p1=strstr(buf,"::");
    strncpy(name,p+3,(int)(p1-(p+3)));
    strcpy(new_buf,p1+2);
}

//切割网址
void http_tok(char *website,char *host,char *url)
{
    char *p=NULL;
    p=strstr(website,"https://");
    if(p!=NULL)
    {
        p=p+8;
     
        while(*p!='/')
        {
            char tmp[300];
            sprintf(tmp,"%s%c",host,*p);
            strcpy(host,tmp);
            p++;
        }
        strcpy(url,p);
        return ;
    }
    p=strstr(website,"http://");
    if(p!=NULL)
    {
        p=p+7;
     
        while(*p!='/')
        {
            char tmp[300];
            sprintf(tmp,"%s%c",host,*p);
            strcpy(host,tmp);
            p++;
        }
        strcpy(url,p);
        return ;
    }
    
}

//建立链接tcp
int api_tcp(char *host)
{
    //通过域名获取ip地址
    struct hostent *getip=gethostbyname(host);
    if(getip==NULL)
    {
        perror("");
        return -1;
    }
    char ip[200];
    strcpy(ip,inet_ntoa(*((struct in_addr *)getip->h_addr_list[0])));
    //创建tcp通信socket
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    
    //设置属性
    struct sockaddr_in addr;
    addr.sin_family=AF_INET;
    addr.sin_port=htons(80);//端口号80
    addr.sin_addr.s_addr=inet_addr(ip);
    
    //建立链接
    int ret=connect(tcp_socket,(struct sockaddr *)&addr,sizeof(addr));
    if(ret<0)
    {
        perror("链接失败");
        return -1;
    }
    return tcp_socket;
}

void *send_list(void *arg)
{
    char name[200]={0};
    strcpy(name,(char *)arg);
    struct people *pos=head->next;
    for(pos=head->next;pos!=head;pos=pos->next)
    {
        if(strcmp(name,pos->name)==0)
        {
            break;
        }
    }
    struct people *ptr=head->next;
    for(ptr=head->next;ptr!=head;ptr=ptr->next)
    {
        //拼接标识符
        char send_buf[300]={0};
        sprintf(send_buf,"@@@:%s",ptr->name);
        write(pos->socket,send_buf,strlen(send_buf));
        usleep(500);
    }
}

/*
*读取发送到服务器的内容
*/
void *read_clien(void *arg)
{
    int *socket=arg;
    struct people *pos=NULL;
    for(pos=head->next;pos!=head;pos=pos->next)
    {
        if(pos->socket==*socket)
        {
            break;
        }
    }
    while(1)
    {
        char buf[1024]={0};
        int size=read(pos->socket,buf,1024);
        if(size<=0)//对方掉线了，删除信息
        {
            //告诉所有在线用户它下线了
            struct people *ptr=head->next;
            for(ptr=head->next;ptr!=head;ptr=ptr->next)
            {
                if(strcmp(pos->name,ptr->name)!=0)
                {
                    char offline_notification[300];
                    sprintf(offline_notification,"#^#:%s",pos->name);
                    write(ptr->socket,offline_notification,strlen(offline_notification));
                }
            }
            close(pos->socket);
            del_list(pos->name);
            return NULL;
        }
        if(strstr(buf,"@^@:"))//上线获取对方名字
        {
            char name[200]={0};
            sscanf(buf,"@^@:%s",name);
            strcpy(pos->name,name);
            //给所有在线的用户发送上线用户的名字
            struct people *ptr=head->next;
            //创建一个线程将所有在线用户的名字发送给新用户（防止挤成一坨）
            pthread_t tid;
            pthread_create(&tid,NULL,send_list,(void *)&pos->name);
            pthread_detach(tid);
            /*
            for(ptr=head->next;ptr!=head;ptr=ptr->next)
            {
                //链表里的所有名字发给新上线的(发送链表)
                char online_notification[300];
                sprintf(online_notification,"@@@:%s",ptr->name);
                write(pos->socket,online_notification,strlen(online_notification));
            }
            */
            //将自己的名字发给所有在线客户
            for(ptr=head->next;ptr!=head;ptr=ptr->next)
            {
                if(strcmp(ptr->name,pos->name)!=0)
                {
                    char online_notification[300];
                    sprintf(online_notification,"@^@:%s",pos->name);
                    write(ptr->socket,online_notification,strlen(online_notification));
                }
            }
        }
        else if(strstr(buf,"#^#:"))//对方下线了,删除信息
        {
            //告诉所有在线用户它下线了
            struct people *ptr=head->next;
            for(ptr=head->next;ptr!=head;ptr=ptr->next)
            {
                if(strcmp(pos->name,ptr->name)!=0)
                {
                    char offline_notification[300];
                    sprintf(offline_notification,"#^#:%s",pos->name);
                    write(ptr->socket,buf,size);
                }
            }
            close(pos->socket);
            del_list(pos->name);
            return NULL;
        }
        else if(strstr(buf,"^^:"))//客户端1给客户端2发送消息
        {
            char name[200]={0};
            char new_buf[1024]={0};
            send_tok(buf,name,new_buf);
            struct people *ptr=head->next;
            for(ptr=head->next;ptr!=head;ptr=ptr->next)
            {
                if(strcmp(ptr->name,name)==0)//找到客户端2的信息
                {
                    //拼接自己的名字和内容
                    char send_buf[2000]={0};
                    sprintf(send_buf,"^^:%s::%s",pos->name,new_buf);
                    write(ptr->socket,send_buf,strlen(send_buf));
                    break;
                }
            }
        }
        else if(strstr(buf,"****"))//发送文件或者图片//获取文件头信息
        {
            //****客户端名****文件名****文件大小****
            //获取发送给对方的名字
            char send_buf[2048]={0};
            char name[200]={0};
            char file_name[200]={0};
            char s_size[200]={0};
            int file_len=0;
            char new_buf[1024]={0};
            //切割字符串
            sscanf(buf,"****%[^****]****%[^****]****%[^****]****%s",name,file_name,s_size,new_buf);
            printf("name=%s\n",name);
            printf("file_name=%s\n",file_name);
            printf("s_size=%s\n",s_size);
            printf("new_buf=%s\n",new_buf);
            printf("第一次内容的大小:%ld\n",strlen(new_buf));
            //将文件大小转换为int
            sscanf(s_size,"%d",&file_len);
            printf("file_len=%d\n",file_len);
            struct people *ptr=head->next;
            for(ptr=head->next;ptr!=head;ptr=ptr->next)//找到对方
            {
                if(strcmp(ptr->name,name)==0)
                {
                    break;
                }
            }
            //将自己的名字写入
            int head_size=strlen(name)+strlen(file_name)+strlen(s_size)+16;//原头信息大小
            printf("head_size=%d\n",head_size);
            
            sprintf(send_buf,"****%s****%s****%s****%s",pos->name,file_name,s_size,buf+head_size);
            int new_head_size=strlen(pos->name)+strlen(file_name)+strlen(s_size)+16;
            printf("new_head_size=%d\n",new_head_size);
            //将头信息写入
            int total_size=(new_head_size+(size-head_size));//新头信息大小+内容大小
            write(ptr->socket,send_buf,total_size);
            
            printf("第一次发送大小:%d\n",total_size);
            while(1)
            {
                //读取后续信息
                size=read(pos->socket,send_buf,2048);
                write(ptr->socket,send_buf,size);
                total_size+=size;
                printf("total_size=%d\n",total_size);
                if(total_size>=file_len)
                {
                    printf("文件发送完毕\n");
                    break;
                }
            }
        }
        else if(strstr(buf,"@$joke$@"))//舔狗日记
        {
            printf("正在请求讲笑话\n");
            char website[1024]="http://api.qingyunke.com/api.php?key=free&appid=0&msg=笑话";
            char host[200]={0};
            char url[200]={0};
            http_tok(website,host,url);//切割网址
            int socket=api_tcp(host);//创建链接
            //定制http请求
            char http[1024];
            sprintf(http,"GET %s HTTP/1.1\r\nHost:%s\r\n\r\n",url,host);
            
            //发送给服务器网站
            write(socket,http,strlen(http));
            //读取信息
            while(1)
            {
                char tmp[4096]={0};
                int down_size=read(socket,tmp,4096);
                if(down_size<=0)
                {
                   break;
                }
                printf("申请到: %s\n",tmp);
                char *p=NULL;
                p=strstr(tmp,"\"content\":");
                if(p!=NULL)
                {
                    char *p1=p+10;
                    while(*p1!='\n')
                    {
                        p1++;
                    }
                    //计算长度
                    p1-=2;//不要}
                    int send_len=(int)(p1-(p+10));
                    char send_tmp[4096]={0};
                    strncpy(send_tmp,p+10,send_len);
                    //拼接标识符
                    char send_buf[5020]={0};
                    sprintf(send_buf,"@$joke$@%s",send_tmp);
                    printf("向客户端发送: %s\n",send_buf);
                    //将信息返回给客户端
                    write(pos->socket,send_buf,strlen(send_buf));
                }
            }
            close(socket);
        }
        else if(strstr(buf,"@$weather$@"))//天气
        {
            //接收地方
            char name[200]={0};
            sscanf(buf,"@$weather$@%s",name);
            char website[1024]={0};
            sprintf(website,"http://api.qingyunke.com/api.php?key=free&appid=0&msg=天气%s",name);
            printf("website=%s\n",website);
            char host[200]={0};
            char url[200]={0};
            http_tok(website,host,url);
            int socket=api_tcp(host);//创建链接
            //定制http请求
            char http[1024];
            sprintf(http,"GET %s HTTP/1.1\r\nHost:%s\r\n\r\n",url,host);
            write(socket,http,strlen(http));
            while(1)
            {
                char tmp[4096]={0};
                int down_size=read(socket,tmp,4096);
                if(down_size<=0)
                {
                    break;
                }
                printf("%s\n",tmp);
                char *p=NULL;
                p=strstr(tmp,"\"content\":");
                if(p!=NULL)
                {
                    char *p1=p+10;
                    while(*p1!='\n')
                    {
                        p1++;
                    }
                    //计算长度
                    p1-=2;//不要}
                    int send_len=(int)(p1-(p+10));
                    char send_tmp[4096]={0};
                    strncpy(send_tmp,p+10,send_len);
                    //拼接标识符
                    char send_buf[5020]={0};
                    sprintf(send_buf,"@$weather$@%s",send_tmp);
                    //将信息返回给客户端
                    write(pos->socket,send_buf,strlen(send_buf));
                }
            }
            close(socket);
        }
    }
}

int main(int argc,char *argv[])
{
     printf("服务器正在启动，请稍后\n");
    //初始化线程池
    struct thread_pool *pool=malloc(sizeof(struct thread_pool));  
    init_thread_pool(pool,20);
    sleep(2);//确保线程初始化完毕
    
    printf("服务器启动完毕!\n");
    
    //创建tcp通信socket
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(tcp_socket<0)
    {
        perror("");
        return -1;
    }
    
    //开启端口复用
    int  on=1;
    setsockopt(tcp_socket,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
    
    //设置服务器属性
    struct sockaddr_in addr;
    addr.sin_family=AF_INET;
    addr.sin_port=htons(6697);
    addr.sin_addr.s_addr=inet_addr(MY_IP);
    
    //绑定
    int ret= bind(tcp_socket, (struct sockaddr *)&addr,sizeof(addr));
    if(ret<0)
    {
        perror("");
        return -1;
    }
    
    //设置监听
    ret=listen(tcp_socket,5);
    if(ret<0)
    {
        perror("");
        return -1;
    }
    
    //初始化在线客户链表
    head=malloc(sizeof(struct people));
    head->next=head;
    head->prev=head;
    
    //唤醒一个线程作为自己发送公告的平台
    add_task(pool,server_func,NULL);
    
    
    
    while(1)
    {
        //等待链接并保存对方ip和端口号
        struct sockaddr_in clien_addr;
        int addr_len=sizeof(clien_addr);
        int new_socket=accept(tcp_socket,(struct sockaddr *)&clien_addr,&addr_len);
        if(new_socket<0)
        {
            perror("");
            continue;
        }
        //转换对方信息
        char ip[200];
        strcpy(ip,inet_ntoa(clien_addr.sin_addr));
        int port=ntohs(clien_addr.sin_port);
        //将对方信息写入链表
        add_into_list(ip,port,new_socket);
        //找到链表里自己的内容
        struct people *pos=head->next;
        for(pos=head->next;pos!=head;pos=pos->next)
        {
            if(pos->socket==new_socket)
            {
                break;
            }
        }
        //唤醒一个线程读取信息
        add_task(pool,read_clien,(void *)&pos->socket);//防止new_socket被释放
    }
}

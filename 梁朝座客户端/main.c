/**********************************************************************
@	名称：  小区信息管理系统
@	参数：  无
@	返回值：无
@   说明：  小区信息管理系统客户端，上线通知服务器，自动获取今天天气情况，
@            与其他小区可以通信，无通信时，自动播放生活中安全知识视频
========================================================================*/
#include "font.h"
#include "thread_pool.h" 
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> 
#include <unistd.h>
#include <pthread.h>
#include <string.h>

void weather(char *arg);

font *f;                           //字库
int tcp_socket;   					//tcp协议
char video_name[10][100]={0};  		 //保存视频文件路径名 
FILE* mp;                     		//popen管道描述符
thread_pool *pool;            		//线程池管理节点
int no_off=1;
int num=0;
char friend_name[10][100]={0}; 
int friend_i=0;
int hui_num=0;
char name_na[100] = {0};
int gg=1;


int open_fd(char *dir_file)
{
	
	int fd_ret = open(dir_file, O_RDWR);
	if (-1 == fd_ret)
	{
		perror("open ts failed");
		return -1;
	}
	return fd_ret;
}

int show_bmp(int x,int y,char *pic_name)
{
	int bmp_fd = open(pic_name,O_RDWR);
	if(bmp_fd == -1)
	{
		perror("open bmp failed");
		return -1;
	}
	/*****************获取图片的大小***************/
	char head[54]={0};
	read(bmp_fd,head,sizeof(head));
	int len  = head[21]<<24 | head[20]<<16 | head[19]<<8 | head[18]; //宽
	int higt = head[25]<<24 | head[24]<<16 | head[23]<<8 | head[22];  //高
	
	
	char buf[higt*len*3];
	read(bmp_fd,buf,sizeof(buf));
	lseek(bmp_fd,54,SEEK_SET);
	
	unsigned int tmp;
/***********bmp像素转换成lcd像素*************/	
	unsigned int bmp_buf[len*higt];
	unsigned char A,R,G,B;
	int i,j;
	for(i=0;i<len*higt;++i)
	{
		A=0X00;
		B=buf[i*3];
		G=buf[i*3+1];
		R=buf[i*3+2];
		
		bmp_buf[i]=A<<24 | R<<16 | G<<8 | B;
	}
	
/***************实现图片反转*******************/	
	for(i=0;i<higt/2;++i)
	{
		for(j=0;j<len;++j)
		{
				//把第i行，第j列的像素点跟 第479-i行，第j列的像素点进行交换
 				tmp = bmp_buf[len*i+j];
 				bmp_buf[len*i+j] = bmp_buf[len*(higt-i)+j];
 				bmp_buf[len*(higt-i)+j] = tmp;	
		}
	}


	
	int lcd_fd = open("/dev/fb0",O_RDWR);
	if(lcd_fd == -1)
	{
		perror("open lcd failed:");
		return -1;
	}
	
	//将LCD映射到内存空间
	unsigned int *lcdmap = mmap(NULL,800*480*4,PROT_WRITE|PROT_WRITE,MAP_SHARED,lcd_fd,0);
	if(lcdmap == MAP_FAILED)
	{
		perror("mmap failed:");
		return -1;
	}
	
	for(i=y;i<higt+y && i<480;i++)
	{
		for(j=x;j<len+x && j<800;j++)
		{
			lcdmap[i*800+j] = bmp_buf[(i-y)*len+(j-x)] ;
		}
		
	}
	//解除映射   关闭文件 
	munmap(lcdmap,800*480*4);
	close(lcd_fd);
	close(bmp_fd);
}
int  socket_loin()
{
	//1.创建 TCP  通信协议
        tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
			if(tcp_socket < 0)
			{
				perror("tcp_socket");
				return -1; 
			}
			
		//设置链接的服务器地址信息 
		struct sockaddr_in  addr;  
		addr.sin_family   = AF_INET; //IPV4 协议  
		addr.sin_port     = htons(6697); //端口
		addr.sin_addr.s_addr = inet_addr("192.168.22.209"); //服务器的IP 地址
		//链接服务器 
		int ret=connect(tcp_socket,(struct sockaddr *)&addr,sizeof(addr));
		if(ret < 0)
		{
			perror("connect");
			return -1;
		}
		printf("连接服务器成功\n");
		//把登录信息发给服务器
		char buf_loin[100]={"@^@:翻斗花园"};
		write(tcp_socket,buf_loin,sizeof(buf_loin)-1);
		return 0;
}

void *font_ziku(int si,int x1,int y1,int x2,int y2,char *buf)
{
	//申请空间
	struct LcdDevice* lcd = malloc(sizeof(struct LcdDevice));
	if(lcd == NULL)
	{
		return NULL;
	} 
	//1打开设备
	lcd->fd = open("/dev/fb0", O_RDWR);
	if(lcd->fd < 0)
	{
		perror("open lcd fail");
		free(lcd);
		return NULL;
	}
	//映射
	lcd->mp = mmap(NULL,800*480*4,PROT_READ|PROT_WRITE,MAP_SHARED,lcd->fd,0);
	
	//打开字体	
	font *f = fontLoad("/usr/share/fonts/DroidSansFallback.ttf");
	  
	//字体大小的设置
	fontSetSize(f,si);
	//创建一个画板（点阵图）
	bitmap *bm = createBitmapWithInit(x1,y1,4,getColor(0,255,255,255)); //也可使用createBitmapWithInit函数，改变画板颜色
	
	//将字体写到点阵图上
	fontPrint(f,bm,0,0,buf,getColor(0,255,0,0),0);
	
	//把字体框输出到LCD屏幕上
	show_font_to_lcd(lcd->mp,x2,y2,bm);
	
	//关闭点阵图
	destroyBitmap(bm);
	
}


void *Gong_Gao(void *arg)
{	
		
		char str2[1024] = {0};
		char sss[1024] = {0};
		strcpy(str2,(char *)arg);
		strcpy(sss,(str2+strlen("&*&*:")));
		while(1)
		{
			printf("sss=%s\n",sss);
			
			if(strlen(sss)>30)
			{
				char buf[100]={0};
				strncpy(buf,sss,30);
				font_ziku(40,540,45,100,405,buf);
				sleep(3);
				memset(buf,0,100);
				strcpy(buf,sss+30);
				font_ziku(40,540,45,100,405,buf);
			}
			else
			{
				font_ziku(40,540,45,100,405,str2+strlen("&*&*:"));
			}
		
			sleep(3);
			printf("%d\n",gg);
			if(gg%2)
			{
				gg++;
				printf("我退出了\n");
				break;
			}
			
		}
		
			
}
//上线
void *Up(void *arg)
{
		char str[200]={0};
		strcpy(str,(char *)arg);
	    //上线字库接口
		char buf_up[200]={0};
		sprintf(buf_up,"%s已上线",str+strlen("@^@:"));
		font_ziku(30,200,50,600,10,buf_up);
		sleep(3);
		
	
}

//下线
void *down(void *arg)
{
		char str[200]={0};
		strcpy(str,(char *)arg);
	    //上线字库接口
		char buf_up[200]={0};
		sprintf(buf_up,"%s已下线",str+strlen("#^#:"));
		font_ziku(30,200,50,600,10,buf_up);
		sleep(3);
		
}

void get_file(char *file1,int r)
{
	char name[200]={0};
	char file_name[200]={0};
	char size[100]={0};
	char buf_pin[1024]={0};
	int down_size = 0;
	sscanf(file1,"****%[^****]****%[^****]****%[^****]****",name,file_name,size);
	int siz=atoi(size);
	printf("name:%s\n");
	printf("file_name:%s\n",file_name);
	printf("size:%d\n",siz);
	int fd_1 = open(file_name,O_RDWR|O_CREAT|O_TRUNC,0777);
	sprintf(buf_pin,"****%s****%s****%s****",name,file_name,size);
	int sizeo = strlen(buf_pin);
	printf("r=%d   sizeo=%d\n",r,sizeo);
	//开始写入
	write(fd_1,file1+sizeo,r-sizeo);
	down_size+=r-sizeo;
	char buf[1024]={0};
	while(1)
	{
		memset(buf,0,sizeof(buf));
		int ret0 = read(tcp_socket,buf,sizeof(buf));
		write(fd_1,buf,ret0);
		down_size+=ret0;
		
		if(down_size == siz)
		{
			printf("down=%d,下载完毕\n",down_size);
			break;
		}
	}
}

//通信
void *receive(void *arg)
{
	//读取服务器发送过来的数据 
	char buf[1024]={0}; 
	while(1)
	{
		memset(buf,0,sizeof(buf));
		int ret_read = read(tcp_socket,buf,sizeof(buf));
		printf("buf=%s\n",buf);
		
		char *str_0 = strstr(buf,"@@@:");
		if(str_0 != NULL)
		{
			char buf_str[1024]={0};
			strcpy(buf_str,str_0+strlen("@@@:"));
			if(NULL != strstr(buf_str,"@@@:"))
			{
				char bufq[1024]={0};
				char bufw[1024]={0};
				
				sscanf(buf_str,"%[^@@@:]@@@:%s",bufq,bufw);
				
				if(strcmp(bufq,"翻斗花园"))
				{
					strcpy(friend_name[friend_i++],bufq);       //添加进数组列表
				}
				if(strcmp(bufw,"翻斗花园"))
				{
					strcpy(friend_name[friend_i++],bufw);       //添加进数组列表
				}
			}
			else if(strcmp(buf_str,"翻斗花园"))
			{
				strcpy(friend_name[friend_i++],buf_str);       //添加进数组列表
			}
		}
		
		
		
		
		//上线
		char *str = strstr(buf,"@^@:");
		if(NULL != str)
		{
			if(strcmp(str+strlen("@^@:"),"翻斗花园"))                  //自己的不添加
			{
				add_task(pool,Up,(void *)str); 
				strcpy(friend_name[friend_i++],str+strlen("@^@:"));       //添加进数组列表
				usleep(1*1000);
			} 
		}
		//下线
		char *str3 = strstr(buf,"#^#:");
		if(NULL != str3)
		{
			add_task(pool,down,(void *)str3); 
			int i=0;
			for(i=0;i<friend_i;i++)
			{
				if(!strcmp(friend_name[i],str3+sizeof("#^#")))
				{
					int j=0;
					for(j=i;j<friend_i;j++)
					{
						stpcpy(friend_name[j],friend_name[j+1]);
					}
					friend_i--;
					break;
				}
			}
			usleep(1*1000);
		}
		
		//信息
		char *str1 = strstr(buf,"^^:");
		if(NULL != str1)
		{
			char content[1024]= {0};
			char buf_rev[100] = {0};
			char buf_con[1024]= {0};
			
			sscanf(str1, "^^:%[^::]::%s", name_na, content);
			sprintf(buf_rev,"%s来信息",name_na);
			sprintf(buf_con,"%s：%s",name_na,content);
			font_ziku(30,200,50,600,10,buf_rev);
			printf("%s\n",name_na);
			system("killall -STOP mplayer");           //暂停播放             
			
			sleep(1);
			show_bmp(0,0,"1.bmp"); 
			font_ziku(35,80,40,280,20,name_na);
			font_ziku(30,500,50,100,100,buf_con);
			hui_num = 1;
		}
		
		 //公告
		char *str2 = strstr(buf,"&*&*:");    
		if(NULL != str2)
		{
			
			
			usleep(1*1000);
			add_task(pool,Gong_Gao,(void *)str2); 
			gg++;
			usleep(1*1000);
			
		}
		//天气
		char *str4 = strstr(buf,"@$weather$@");    
		 if(NULL != str4)
		{
			weather(str4);
		}
		
		char *str5 = strstr(buf,"****");    
		if(NULL != str5)
		{
			font_ziku(30,200,50,600,10,"你接到文件");
			
			get_file(str5,ret_read);
		}
		
	}
}


void *video_playing(void *arg)
{
		
	if(access("/fifo",F_OK))
	{
		mkfifo("/fifo", 0777); 
	}
	
	mp=popen("mplayer -quiet -slave -zoom -x 596 -y 400 -input file=/fifo  ssjs.avi" ,"r"); 
	
}


int Get_slide()
{
	int fd = open_fd("/dev/input/event0");               //打开屏幕
	int x1,x2,y1,y2;
	struct input_event xy;
	int flag=0;
	
		while(1)   //获取起点坐标
		{
			read(fd,&xy,sizeof(xy));
			if (xy.type == EV_ABS && xy.code == ABS_X && flag == 0)
			{
				x1 = xy.value*800 / 1024;
				flag = 1;
			}
			if (xy.type == EV_ABS && xy.code == ABS_Y && flag == 1)
			{
				y1 = xy.value*480 / 600;
				flag = 2;
			}
			if (flag == 2)
			{
				printf("Start_XY(%d,%d)\n", x1, y1);
				flag = 0;
				break;
			}
		}
		/*获取滑动过程中终点位置的坐标*/
		x2 = x1;
		y2 = y1;		//把起点坐标赋值给终点坐标 ==> 如果没有滑动屏幕，那么起点坐标就等于终点坐标
		while(1)	//获取滑动过程中起点坐标的代码
		{
			read(fd, &xy, sizeof(xy));
			if (xy.type == EV_ABS && xy.code == ABS_X)
			{
				x2 = xy.value*800 / 1024;
			}
			if (xy.type == EV_ABS && xy.code == ABS_Y)
			{
				y2 = xy.value*480 / 600;
			}
			//手指离开屏幕时，采集的坐标才是最后一个坐标
			if (xy.type == EV_KEY && xy.code == BTN_TOUCH && xy.value == 0)
			{
				printf("End_XY(%d,%d)\n", x2, y2);
				break;
			}	
		}
		
		if(x2>650 && x2<800 && y2>70 && y2<120)
				{
					printf("文件箱\n");
					close(fd);
					return 1;
				}
				
		if(x2>650 && x2<800 && y2>140 && y2<190)
				{
					printf("信息接收\n");
					close(fd);
					return 2;
				}
		if(x2>280 && x2<330 && y2>360 && y2<400)
				{
					printf("回复按钮\n");
					close(fd);
					return 3;
				}
				
		if(x2>650 && x2<800 && y2>210 && y2<260)
				{
					printf("天气按钮\n");
					close(fd);
					return 4;
				}
		
		
}

int friend_slide()
{
	int up_down=1;
	int fd = open_fd("/dev/input/event0");               //打开屏幕
	int x1,x2,y1,y2;
	struct input_event xy;
	int flag=0;
	while(1)
	{
			show_bmp(0,0,"2.bmp");
			int j = 0;
			for(j=0;j<3;j++)
			{
				printf("好友%s\n",friend_name[3*up_down-3+j]);
				font_ziku(25,140,30,660,j*60+60,friend_name[3*up_down-3+j]);
			}
		while(1)   //获取起点坐标
		{
			read(fd,&xy,sizeof(xy));
			if (xy.type == EV_ABS && xy.code == ABS_X && flag == 0)
			{
				x1 = xy.value*800 / 1024;
				flag = 1;
			}
			if (xy.type == EV_ABS && xy.code == ABS_Y && flag == 1)
			{
				y1 = xy.value*480 / 600;
				flag = 2;
			}
			if (flag == 2)
			{
				//printf("Start_XY(%d,%d)\n", x1, y1);
				flag = 0;
				break;
			}
		}
		/*获取滑动过程中终点位置的坐标*/
		x2 = x1;
		y2 = y1;		//把起点坐标赋值给终点坐标 ==> 如果没有滑动屏幕，那么起点坐标就等于终点坐标
		while(1)	//获取滑动过程中起点坐标的代码
		{
			read(fd, &xy, sizeof(xy));
			if (xy.type == EV_ABS && xy.code == ABS_X)
			{
				x2 = xy.value*800 / 1024;
			}
			if (xy.type == EV_ABS && xy.code == ABS_Y)
			{
				y2 = xy.value*480 / 600;
			}
			//手指离开屏幕时，采集的坐标才是最后一个坐标
			if (xy.type == EV_KEY && xy.code == BTN_TOUCH && xy.value == 0)
			{
				printf("End_XY(%d,%d)\n", x2, y2);
				break;
			}	
		}
		
		
		
		if(x2<80 && x2>0 && y2<50 &&y2>0)
		{
			close(fd);
			return 0;
		}
		
		
		int t_num = 0;
		float f_num = (float)friend_i/3;
		
		int i=0;
		for(i=0;i<friend_i;i++)
		{
			if(f_num<=i)
			{
				t_num=i;
				
				break;
			}
		}
		
		printf("t_num=%d\n",t_num);
		
		
		if (y2 < y1 && (y2-y1)*(y2-y1) > (x2-x1)*(x2-x1))//上滑
		{
			printf("Up Slide\n");
			if(up_down<t_num)
			{
				up_down++;
			}
		}

		if (y2 > y1 && (y2-y1)*(y2-y1) > (x2-x1)*(x2-x1))//下滑
		{
			printf("Down Slide\n");
			if(up_down>1)
			{
				up_down--;
			}
		}
			
		if(x2<790 && x2>650 && y2<100 &&y2>60 && up_down<=t_num)    //好友栏1
		{ 
			char buf_biao[200]={0};
			char buf_scan[200]={0};
			char buf_ping[200]={0};
			char buf_socke[200]={0};
			
			sprintf(buf_biao,"==%s==",friend_name[up_down*3-3]);
			font_ziku(30,150,40,250,10,buf_biao);
			printf("请输入对%s的信息\n",friend_name[up_down*3-3]);
			scanf("%s",buf_scan);
			sprintf(buf_ping,"翻斗花园:%s",buf_scan);
			font_ziku(40,500,50,50,100,buf_ping);
			sprintf(buf_socke,"^^:%s::%s",friend_name[up_down*3-3],buf_scan);
			write(tcp_socket,buf_socke,sizeof(buf_socke)-1);
			sleep(5);
			
		}
		
		if(x2<790 && x2>650 && y2<150 &&y2>110 && up_down<=t_num)    //好友栏2
		{
			
			char buf_biao[200]={0};
			char buf_scan[200]={0};
			char buf_ping[200]={0};
			char buf_socke[200]={0};
			
			sprintf(buf_biao,"==%s==",friend_name[up_down*3-2]);
			font_ziku(30,150,40,250,10,buf_biao);
			
			printf("请输入对%s的信息\n",friend_name[up_down*3-2]);
			scanf("%s",buf_scan);
			sprintf(buf_ping,"翻斗花园:%s",buf_scan);
			font_ziku(40,500,60,50,100,buf_ping);
			
			sprintf(buf_socke,"^^:%s::%s",friend_name[up_down*3-2],buf_scan);
			write(tcp_socket,buf_socke,sizeof(buf_socke)-1);
			sleep(5);
		}
			
		if(x2<790 && x2>650 && y2<200 &&y2>160 && up_down<=t_num)     //好友栏3
		{
			char buf_biao[200]={0};
			char buf_scan[200]={0};
			char buf_ping[200]={0};
			char buf_socke[200]={0};
			
			sprintf(buf_biao,"==%s==",friend_name[up_down*3-1]);
			font_ziku(30,150,40,250,10,buf_biao);
			printf("请输入对%s的信息\n",friend_name[up_down*3-1]);
			scanf("%s",buf_scan);
			sprintf(buf_ping,"翻斗花园:%s",buf_scan);
			font_ziku(40,500,50,50,100,buf_ping);
			sprintf(buf_socke,"^^:%s::%s",friend_name[up_down*3-1],buf_scan);
			write(tcp_socket,buf_socke,sizeof(buf_socke)-1);
			sleep(5);
		}
	}
}

int file_slide()
{
		char buf[1024]={0}; 
		char hair_user_name[1024] = {0};
		char hair_file_name[1024] = {0};
		show_bmp(0,0,"2.bmp");
		printf("请输入收件人和文件名\n");
		font_ziku(40,300,50,200,30,"请输入收件人和文件名");
		scanf("%s %s", hair_user_name, hair_file_name);
		sprintf(buf,"%s:",hair_user_name);
		font_ziku(40,200,50,250,150,buf);
		
		//打开文件
		int fd2= open(hair_file_name, O_RDWR);
		if(fd2 == -1)
		{
			font_ziku(40,250,50,350,280,"没有该文件");
			perror("文件打开失败！\n");
			return 0;
		}
		
		//获取文件大小
		int file_size = lseek(fd2, 0, SEEK_END);
		lseek(fd2, 0, SEEK_SET);
		
		char file_all[54] = {0};
		sprintf(file_all,"****%s****%s****%d****", hair_user_name, hair_file_name, file_size);
		write(tcp_socket, file_all, strlen(file_all));
		
		int all = 0;
		while(1)
		{
			char buf[4096] = {0};
			int size = read(fd2, buf, 4096);	//读取文件内容到buf里面
			if(size <= 0)
			{
				printf("文件发送完成\n");
				font_ziku(40,250,50,350,280,"文件发送完毕");
				break;
			}
			all += size;
			write(tcp_socket, buf, size);	//将读取到的内容写给服务器
		}
		printf("已发送文件大小：%d,文件总大小：%d\n", all, file_size);
		close(fd2);
}

//get天气
void weather(char *arg)
{
		system("killall -STOP mplayer");
		
		char weather_1[1024] = {0};
		char weather[1024] = {0};
		char weather1[1024] = {0};
		char weather2[1024] = {0};
		char weather3[1024] = {0};
		char weather4[1024] = {0};
		char weather5[1024] = {0};
		char weather6[1024] = {0};
		char weather7[1024] = {0};
		char receive_buf[1024]={0};
		
		strcpy(receive_buf,arg);
		show_bmp(0,0,"2.bmp");
		font_ziku(30,150,40,250,10,"天气情况");
		sscanf(receive_buf, "@$weather$@\"%s ：%[^，]，%[^{]{br}%[^{]{br}%[^{]{br}%[^{]{br}%[^{]{br}%[^\"]", weather, weather7, weather1, weather2, weather3, weather4, weather5, weather6);
		sprintf(weather_1, "%s：%s\n", weather, weather7);
		
		font_ziku(30,500,35,20,100,weather_1);
		
		font_ziku(30,550,35,10,140,weather1);
		font_ziku(30,550,35,10,180,weather2);
		font_ziku(30,550,35,10,220,weather3);
		font_ziku(30,550,35,10,260,weather4);
		font_ziku(30,550,35,10,300,weather5);
		font_ziku(30,550,35,10,340,weather6);
		sleep(5);
		system("killall -CONT mplayer");
		show_bmp(0,0,"1.bmp");
}



int main()
{
	show_bmp(0,0,"1.bmp");
	
	socket_loin();
	
	printf("登录成功\n");

	pool=malloc(sizeof(thread_pool));

	init_pool(pool,20);                     //初始化线程池	
	
	add_task(pool,receive,NULL);            //接收通信
	add_task(pool,video_playing,NULL);      //播放视频
	
	int ret_Get_slide;                       //接收屏幕坐标返回值
	int friend_no=0;
	while(1)
	{		
		
		//获取屏幕坐标
		ret_Get_slide = Get_slide();
		
		 //发文件
		if(ret_Get_slide == 1)     
		{
			system("killall -STOP mplayer");
			file_slide();
			sleep(6);
			show_bmp(0,0,"1.bmp");
			system("killall -CONT mplayer");		//继续信号
		}
		
		//发信息
		if(ret_Get_slide == 2 && friend_no==0)
		{
				friend_no=1;
				//发送一个暂停信号  
				system("killall -STOP mplayer");
				show_bmp(0,0,"2.bmp");
				
				
				int ret_friend;
				while(1)
				{
					
				    ret_friend = friend_slide();
					if(ret_friend == 0)
					{
						friend_no=0;
						break;	
					}
				}
				show_bmp(0,0,"1.bmp");
				system("killall -CONT mplayer");		//继续信号
		}
		
		if(ret_Get_slide == 3 && hui_num==1)
		{
				printf("请回复\n");
				char buf_hui[1024]={0};
				char buf_fu[1024]={0};
				scanf("%s",buf_hui);
				sprintf(buf_fu,"^^:%s::%s",name_na,buf_hui);
				write(tcp_socket,buf_fu,strlen(buf_fu));
				
		}
		
		
		if(ret_Get_slide == 4 )
		{
			write(tcp_socket,"@$weather$@广州天气",strlen("@$weather$@广州天气"));
			sleep(2);
		}
		
	}
	//关闭通信 
	close(tcp_socket);
	fontUnload(f);   //关闭字库
}

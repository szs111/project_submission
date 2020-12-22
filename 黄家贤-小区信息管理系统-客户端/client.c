/*
	名称：小区信息管理系统（客户端）
	作者：黄家贤
	功能：
		1、上线后连接到服务器中
		2、服务器发布最新的小区公告到客户端中
		3、客户端可以获取一些网络信息，如：天气，新闻等
		4、客户端可以选择在线的其他客户端进行通信
		附加：实现图片或者文件的转发
	版本：1.0
	时间：2020/12/17
*/

#include "head.h"
#include "font.h"
#include "thread_pool.h"


int tcp_socket;				//TCP通信协议
struct sockaddr_in addr;	//设置连接服务器地址信息
struct thread_pool *pool= NULL;	//定义一个线程池管理结构体
font *f= NULL;					//打开字体
struct LcdDevice* lcd;
struct user * head = NULL;		//初始化一条循环链表
int x,y;				//触摸屏真正的坐标
int ts_fd;				//触摸屏文件


//用来接收服务器发送的消息
void *acc_server(void *arg)
{
	while(1)
	{
		char receive_buf[1024] = {0};
		int w_size = read(tcp_socket, receive_buf, 1024);
		if(w_size <= 0)
		{
			printf("服务器断开！");
			exit(0);
		}
		
		if( strstr(receive_buf, "&*&*:"))				//服务器发公告
		{
			char notice[1024] = {0};
			sscanf(receive_buf, "&*&*:%s", notice);
			printf("接收到服务器发来的公告：%s\n", notice);
			char splicing_buf[1024] = {0};
			// sprintf(splicing_buf, "公告：%s 0 0", notice);
			sprintf(splicing_buf, "公告：%s", notice);
			// printf("%s\n", splicing_buf);
			font_(splicing_buf, 40, 620, 50 , 90, 0);
		}
		else if(strstr(receive_buf, "^^:"))				//服务器转发消息
		{
			char name[1024] = {0};
			char content[1024] = {0};
			sscanf(receive_buf, "^^:%[^::]::%s", name, content);
			
			printf("客户端：%s 发来消息：%s\n", name, content);
			
			char splicing_buf[1024] = {0};
			sprintf(splicing_buf, "正在与%s聊天： ", name);
			font_(splicing_buf, 40, 600, 41 , 100, 100);
			
			sprintf(splicing_buf, "%s ", content);
			font_(splicing_buf, 40, 600, 41 , 100, 141);
			
		}
		else if(strstr(receive_buf, "@@@:"))			//好友列表
		{
			char notice[1024] = {0};
			sscanf(receive_buf, "@@@:%s", notice);
			printf("列表：%s\n", notice);
			
			//创建一个新节点new
			struct user * new = new_user(notice);
			if(new == NULL)
			{
				perror("结点内存申请失败");
				exit(0);
			}
			
			//将新节点插入到链表的尾部
			add_list_tail(new);
			
		}
		else if(strstr(receive_buf, "@^@:"))			//某某用户上线了
		{
			char notice[1024] = {0};
			sscanf(receive_buf, "@^@:%s", notice);
			printf("%s上线了\n", notice);
			
			//创建一个新节点new
			struct user * new = new_user(notice);
			if(new == NULL)
			{
				perror("结点内存申请失败");
				exit(0);
			}
			
			//将新节点插入到链表的尾部
			add_list_tail(new);
			
			char splicing_buf[1024] = {0};
			sprintf(splicing_buf, "%s上线了", notice);
			font_(splicing_buf, 40, 600, 41 , 250, 50);
			sleep(1);
			bmp_show("blank.bmp", 600, 48, 100, 40);
		}
		else if(strstr(receive_buf, "#^#:"))			//某某用户下线了
		{
			char offline[1024] = {0};
			sscanf(receive_buf, "#^#:%s", offline);
			printf("%s下线了\n", offline);
		
			del(offline);
	
			char splicing_buf[1024] = {0};
			sprintf(splicing_buf, "%s下线了", offline);
			font_(splicing_buf, 40, 600, 41 , 250, 50);
			sleep(1);
			bmp_show("blank.bmp", 600, 48, 100, 40);
		}
		else if(strstr(receive_buf, "****"))			//用于接收服务器发送过来的文件
		{
			char *p = strstr(receive_buf, "****");		//指向首地址
			char collect_user_name[1024] = {0};
			char collect_file_name[1024] = {0};
			char size_f[1024] = {0};
			char tex[1024] = {0};
			
				//				**** 用户名	**** 文件名	****  大小	****   
			sscanf(receive_buf,"****%[^****]****%[^****]****%[^****]****", collect_user_name, collect_file_name, size_f);
			
			sprintf(tex, "1%s", collect_file_name);
			
			printf("用户名：%s\n", collect_user_name);
			printf("文件名：%s\n", tex);
		
			int file_size =atoi(size_f);		//文件大小
			printf("文件大小：%d\n", file_size);
			
			int total_length = 16+strlen(collect_user_name)+ strlen(collect_file_name)+ strlen(size_f);	//总长度
			printf("头信息总长度：%d\n", total_length);
			
			//新建一个文件 
			int  new_fd = open(tex,O_RDWR|O_CREAT|O_TRUNC,0777);
			if(new_fd < 0)
			{
				 perror("文件创建失败！\n");
				 return 0;
			}
		
			char beleft_buf[1024] = {0};
			strcpy(beleft_buf, p+ total_length);		//指向头信息的末尾
			int all = sizeof(receive_buf)-total_length;
			
			write(new_fd, beleft_buf, all);		//将第一次读取到的除头结点外的信息写入文件中
			
			printf("第一次剩下的长度：%d\n",all);
			
			char buf[4096] = {0};
			
			while(1)
			{	
				bzero(buf,sizeof(buf));
				int size = read(tcp_socket, buf, sizeof(buf));	//读取文件内容到buf里面
			
				all += size;
				// printf("正在接收：%d,已接收文件大小:%d,文件总大小：%d\n",size, all, file_size);
				write(new_fd, buf, size);	//将读取到的内容写入文件
				
				if(all == file_size)
				{
					printf("已接收文件大小:%d,文件总大小：%d\n", all, file_size);
					printf("文件接收完成！\n");
					close(new_fd);
					break;
				}
			}
		}
		else if(strstr(receive_buf, "@$weather$@"))		//接收服务器发来的天气情况
		{
			char weather_1[1024] = {0};
			char weather[1024] = {0};
			char weather1[1024] = {0};
			char weather2[1024] = {0};
			char weather3[1024] = {0};
			char weather4[1024] = {0};
			char weather5[1024] = {0};
			char weather6[1024] = {0};
			char weather7[1024] = {0};
		
			sscanf(receive_buf, "@$weather$@\"%s ：%[^，]，%[^{]{br}%[^{]{br}%[^{]{br}%[^{]{br}%[^{]{br}%[^\"]", weather, weather7, weather1, weather2, weather3, weather4, weather5, weather6);
		
			sprintf(weather_1, "%s：%s\n", weather, weather7);
			printf("%s\n", weather_1);
			font_(weather_1, 35, 740, 40 , 30, 100);
			printf("%s\n", weather1);
			font_(weather1, 35, 735, 40 , 30, 140);
			printf("%s\n", weather2);
			font_(weather2, 35, 740, 40 , 30, 180);
			printf("%s\n", weather3);
			font_(weather3, 35, 740, 40 , 30, 220);
			printf("%s\n", weather4);
			font_(weather4, 35, 740, 40 , 30, 260);
			printf("%s\n", weather5);
			font_(weather5, 35, 740, 40 , 30, 300);
			printf("%s\n", weather6);
			font_(weather6, 35, 740, 40 , 30, 340);
		}
		else if(strstr(receive_buf, "@$joke$@"))			//获取服务器发来的笑话
		{
			printf("服务器发来的消息：%s\n", receive_buf);
			char title[1024] = {0};
			char content[1024] = {0};
			sscanf(receive_buf, "@$joke$@\"★ %[^{]{br}%[^{]", title, content);
			printf("笑话标题：%s\n内容：%s\n", title, content);
		}
		else		//正常接收服务器的消息
			printf("接收到服务器发来的消息：%s\n", receive_buf);
	}
}



//TCP通信协议
int tcp()
{
	//1、创建TCP通信协议
	tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(tcp_socket < 0)
	{
		perror("通信协议创建失败！\n");
		return -1;
	}
	//设置链接服务器地址信息
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("192.168.22.206");
	// addr.sin_addr.s_addr = inet_addr("192.168.22.213");
	// addr.sin_addr.s_addr = inet_addr("192.168.22.33");
	addr.sin_port = htons(6697);
	
	//2、链接服务器
	int ret = connect(tcp_socket, (struct sockaddr *)&addr, sizeof(addr));
	if(ret <0)
	{
		perror("");
		return -1;
	}
	printf("连接服务器成功！\n");
	
	char notice[1024] = {"@^@:江户川柯南"};
	write(tcp_socket, notice, sizeof(notice));
	
	
	
	
}

//创建字库
//				字体				字体大小	画板大小x		画板大小y	字体框x		字体框y
int font_(char font_buf[1024], int font_size, int bm_size_x, int bm_size_y, int font_x, int font_y)
{
	//字体大小的设置
	fontSetSize(f,font_size);
	//创建一个画板（点阵图）
	bitmap *bm = createBitmapWithInit(bm_size_x, bm_size_y, 4, getColor(0,255,255,255)); //也可使用createBitmapWithInit函数，改变画板颜色
	//bitmap *bm = createBitmap(288, 100, 4);
	
	//将字体写到点阵图上
	fontPrint(f,bm,0,0,font_buf,getColor(0,255,0,0),0);
	
	//把字体框输出到LCD屏幕上
	show_font_to_lcd(lcd->mp,font_x,font_y,bm);

	//把字体框输出到LCD屏幕上
	// show_font_to_lcd(lcd->mp,200,200,bm);
	
	destroyBitmap(bm);//画板需要每次都销毁 
	// sleep(6);
	// bmp_show("1.bmp", 800, 480, 0, 0);
	
}

//显示图片
//					照片名		宽度			高度		横坐标		纵坐标
int bmp_show(char *bmp_name,int bmp_length,int bmp_height,int bmp_x,int bmp_y)
{
	//1、打开图片文件
	int bmp_fd=open(bmp_name,O_RDONLY);
	if(bmp_fd==-1)
	{
		perror("open bmp error");
		return -1;
	}
	
	//2、跳过54个字节
	lseek(bmp_fd,54,SEEK_SET);
	
	//3、读取图片数据
	char bmp_buf[bmp_length*bmp_height*3];
	read(bmp_fd,bmp_buf,bmp_length*bmp_height*3);
	
	//4、24位转成32位
	int i,j;
	int buf[bmp_length*bmp_height];
	for(i=0;i<bmp_length*bmp_height;i++)
	{
		//对每一个色素点进行移位操作，然后用位或运算
		buf[i]=bmp_buf[i*3]<<0 | bmp_buf[i*3+1]<<8 | bmp_buf[i*3+2]<<16;
	}
	
	//5、图像翻转
	//i代表行，j代表列
	int lcd_buf[bmp_length*bmp_height];
	for(i=0;i<bmp_height;i++)
		for(j=0;j<bmp_length;j++)
			lcd_buf[j+(bmp_height-1-i)*bmp_length]=buf[j+i*bmp_length];		//把buf第一行，放到lcd_buf的最后一行
		
	//6、把图像数据放到lcd屏幕上
	
	for(i=bmp_y;i<bmp_height+bmp_y;i++)
		for(j=bmp_x;j<bmp_length+bmp_x;j++)
		{
			//对于图片数据只需要跳过每行宽度*i
			//对于屏幕，需要跳过每行800*i
			lcd->mp[i*800+j]=lcd_buf[(i-bmp_y)*bmp_length+j-bmp_x];
		}
	
	close(bmp_fd);
	return 0;
}


//初始化开发板
int init_lcd()
{
	//申请空间
	lcd = malloc(sizeof(struct LcdDevice));
	if(lcd == NULL)
	{
		return 0;
	} 
	
	//打开lcd设备
	lcd->fd = open("/dev/fb0", O_RDWR);
	if(lcd->fd == -1)
	{
		perror("open lcd error");
		free(lcd);
		return 0;
	}
	
	//映射内存
	lcd->mp = mmap(NULL, 800*480*4, PROT_READ|PROT_WRITE, MAP_SHARED,lcd->fd, 0);
	if(lcd->mp == (void *)-1)
	{
		perror("mmap error");
		return -1;
	}
	
	//3、打开触摸屏文件
	ts_fd=open("/dev/input/event0",O_RDONLY);
	if(ts_fd==-1)
	{
		perror("open ts error");
		return -1;
	}
	
	//打开字体	
	f = fontLoad("/usr/share/fonts/DroidSansFallback.ttf"); 
}

//获取触摸屏坐标
int get_ts()
{
	//1、读取触摸屏数据
	//定义一个触摸屏结构体类型的变量
	struct input_event input_ts;
	bzero(&input_ts, sizeof(input_ts));
	
	while(1)
	{
		//此处会阻塞等待，等待点击屏幕
		read(ts_fd,&input_ts,sizeof(input_ts));
		
		if(input_ts.type == EV_ABS)
		{
			//横坐标事件
			if(input_ts.code == ABS_X)
			{
				x=input_ts.value*800/1024;
			}
				
			//纵坐标事件
			if(input_ts.code == ABS_Y)
			{
				y=input_ts.value*480/600;
			}
		}
		//判断触摸屏是否属于松手状态，如果是已松手，value为0
		if(input_ts.type == EV_KEY && input_ts.code == BTN_TOUCH)
		{
			if(input_ts.value== 0)
			{
				printf("(%d,%d)\n",x,y);
				break;
			}
		}
	}
}



//释放资源
int close_all()
{
	//解除映射
	munmap(lcd->mp, 800*480*4);
	close(lcd->fd);
	// fontUnload(f);  //字库不需要每次都关闭 
}


int main(int argc, char * argv[])
{
	//初始化开发板
	init_lcd();
	
	//定义一个线程池管理结构体
	pool = malloc(sizeof(struct thread_pool));
	
	//初始化线程池结构体，并往该线程池里添加10个线程
	init_pool(pool, 10);
	
	//初始化一个空的循环链表
	head=malloc(sizeof(struct user));
	head->next=head;
	
	//调用tpc通信
	int ret = tcp();
	if(ret == -1)
	{
		printf("服务器连接失败！\n");
		return 0;
	}
	
	//添加一个任务，用来接收服务器发送的消息
	add_task(pool, acc_server, NULL);	
	
	while(1)
	{
		//显示主页
		bmp_show("1.bmp", 800, 480, 0, 0);
		printf("==========  小区信息管理系统  ==========\n");
		printf("1、聊天\n");
		printf("2、文件传输\n");
		printf("3、天气情况\n");
		printf("========================================\n");
		get_ts();

		if(x>125&& x<275&& y>320&& y<400)			//聊天
		{
			bmp_show("chat.bmp", 800, 480, 0, 0);
			while(1)
			{
				//将要聊天的对象及内容发送给服务器
				char name_buf[1024]= {0};
				char content_buf[1024]= {0};
				char chat[1024] = {0};
				char splicing_buf[1024]={0};
				get_ts();
				
				if(x>320&& x<480&& y>380&& y<410)		//自定义聊天内容
				{
					printf("用法：对方名字 内容\n");
					scanf("%s %s", name_buf, content_buf);
					sprintf(chat, "^^:%s::%s", name_buf, content_buf);
					printf("发给服务器的内容：%s\n", chat);
					write(tcp_socket, chat, 1024);
					sprintf(splicing_buf, "我：%s", content_buf);
					font_(splicing_buf, 40, 600, 41 , 100, 230);
				}
				if(x>595&& x<700&& y>380&& y<410)
				{
					char r[1024] = {0};
					
					printf("好友列表：\n");
					sprintf(r,"好友列表：");
					font_(r, 30, 200, 31 , 500, 100);
					show(head);
					sleep(2);
					bmp_show("chat.bmp", 800, 480, 0, 0);
				}
				if(x>0&& x<85&& y>0&& y<60)
				{
					break;
				}
			}
		}
		else if(x>320&& x<475&& y>320&& y<400)			//发送文件
		{	
			bmp_show("file_transfer.bmp", 800, 480, 0, 0);
			while(1)
			{
				//获取当前目录的文件
				//打开当前目录
				DIR *dp = opendir("./");
				if(dp == NULL)
				{
					perror("");
					exit(0);
				}
				struct dirent *msg;
				int a=135;
				font_("当前目录文件：", 30, 200, 31 , 100, 100);
				while(1)
				{
					msg =  readdir(dp);
					if(msg == NULL)
					{
						printf("当前目录读取视频文件完毕！\n");
						break;
					}
					if(strcmp(msg->d_name,".")==0  || strcmp(msg->d_name,"..")==0)
						continue;
					printf("%s\n", msg->d_name);
					
					font_(msg->d_name, 30, 300, 31 , 100, a);
					a+=35;
				}
				get_ts();
				if(x>600&& x<700&& y>390&& y<450)		//发送文件
				{
					printf("用法：对方名字 文件名\n");
					char hair_user_name[1024] = {0};
					char hair_file_name[1024] = {0};
					scanf("%s %s", hair_user_name, hair_file_name);

					//打开文件
					int fd= open(hair_file_name, O_RDWR);
					if(fd == -1)
					{
						perror("文件打开失败！\n");
						return 0;
					}
					
					//获取文件大小
					int file_size = lseek(fd, 0, SEEK_END);
					lseek(fd, 0, SEEK_SET);
					
					char file_all[54] = {0};
					sprintf(file_all,"****%s****%s****%d****", hair_user_name, hair_file_name, file_size);
					write(tcp_socket, file_all, strlen(file_all));
					
					int all = 0;
					while(1)
					{
						char buf[4096] = {0};
						int size = read(fd, buf, 4096);	//读取文件内容到buf里面
						if(size <= 0)
						{
							printf("文件发送完成\n");
							break;
						}
						all += size;
						// printf("正在发送：%d,已发送文件大小：%d,文件总大小：%d\n", size, all, file_size);
						write(tcp_socket, buf, size);	//将读取到的内容写给服务器
					}
					printf("已发送文件大小：%d,文件总大小：%d\n", all, file_size);
					close(fd);
				}
				if(x>0&& x<85&& y>0&& y<60)
				{
					break;
				}
			}
		}
		else if(x>520&& x<675&& y>320&& y<400)		//查询天气
		{
			while(1)
			{
				bmp_show("weather.bmp", 800, 480, 0, 0);
				char weather[1024] = {0};
				char location_weather[1024] = {0};
				
				get_ts();
				if(x>35&& x<145&& y>405&& y<465)
				{
					sprintf(weather,"广州天气");
				}
				if(x>155&& x<265&& y>405&& y<465)
				{
					sprintf(weather,"深圳天气");
				}
				if(x>275&& x<385&& y>405&& y<465)
				{
					sprintf(weather,"揭阳天气");
				}
				if(x>400&& x<510&& y>405&& y<465)
				{
					sprintf(weather,"惠州天气");
				}
				if(x>530&& x<635&& y>405&& y<465)
				{
					sprintf(weather,"北京天气");
				}
				if(x>655&& x<760&& y>405&& y<465)
				{
					printf("用法：XX天气\n");
					scanf("%s", weather);
				}
				if(x>0&& x<85&& y>0&& y<60)
				{
					break;
				}
				
				
				char weather_condition[1024] = {0};		//天气情况
				sprintf(weather_condition,"@$weather$@%s", weather);
				
				write(tcp_socket, weather_condition, sizeof(weather_condition));
				printf("正在查询%s,请等待!\n", weather);
			}
		}
	}
	
	pthread_exit(NULL);
	
	return 0;
}
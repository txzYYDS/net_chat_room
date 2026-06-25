/*
 *网络聊天服务端程序
 * */
#include "net_connect.h"
#include "common.h"
#include "client_info.h"

#define MAX_CLIENTS 30
#define PORT 8888
#define BUFFER_SIZE 1024


/*
 *	1.创建服务端套接字
 *	2.初始化套接字结构
 *	3.绑定套接字
 *	4.开始监听
 *	5.循环等待客户端连接
 * */
int main() {

	int server_fd;//服务端套接字
	int client_fd;//客户端套接字
       	int max_fd;//io端口复用最大的描述
	int activity;
    	struct sockaddr_in server_addr;//服务端套接字结构
	struct sockaddr_in client_addr;//客户端套接字结构
    	socklen_t addr_len = sizeof(client_addr);
    	char buffer[BUFFER_SIZE];
    
    	// 1. 创建套接字
    	server_fd = socket(AF_INET, SOCK_STREAM, 0);
    	if (server_fd < 0) {
        	perror("socket");
        	return 1;
    	}
    
    	// 2. 绑定套接字
    	memset(&server_addr, 0, sizeof(server_addr));
    	server_addr.sin_family = AF_INET;
    	server_addr.sin_port = htons(PORT);
    	server_addr.sin_addr.s_addr = INADDR_ANY;
    
    	if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        	perror("bind failed\r\n");
        	return 1;
    	}
    
    	// 3. 监听
    	if (listen(server_fd, 5) < 0) {
        	perror("listen failed\r\n");
        	return 1;
    	}

	printf("Chat Room Init Successfully!\r\n");

	
	/*
	 *怎么解决多个客户端数据监听的问题:
	 *	采用IO多路复用
	 *	这里采用的是select方案 --- 适用于小规模文件监听(最多1024)
	 *	FD_ZERO(fd_set *set);           // 清空集合
	 *	FD_SET(int fd, fd_set *set);    // 将 fd 加入集合
	 *	FD_CLR(int fd, fd_set *set);    // 将 fd 从集合移除
	 *	FD_ISSET(int fd, fd_set *set);  // 检查 fd 是否在集合中（就绪）
	 *
	 *
	 *
	 *怎么保存多个用户的信息呢:
	 *	采用链表保存少量用户信息
	 *	不采用数组去保存，数组修改节点麻烦
	 *	这里需要实现一个用户的数据结构
	 * */

	//客户端节点
	struct client_info *client;
	struct client_info *n_client;

	//初始化管理链表
	struct manager_device *manager =  manager_init();

	//主循环
	while(1) {
		fd_set read_fds;//定义集合
		FD_ZERO(&read_fds);//清空集合
		FD_SET(server_fd,&read_fds);//加入监听集合
		max_fd = server_fd;//最大文件描述

		//将所有客户端添加到集合
		/*
		 *遍历链表节点
		 *并将节点加入集合
		 * */
		manager_for_each_client_safe(client,n_client,manager->head,entry) {
			int fd = client->fd;
			if(fd > 0) {
				FD_SET(fd,&read_fds);
				if(fd > max_fd) {
					max_fd = fd;
				}
			}
		}

		//等待活动 --- 遍历文件集合
		activity = select(max_fd + 1,&read_fds,NULL,NULL,NULL);
		if(0 > activity) {
			perror("select\r\n");
			continue;
		}

		//有新的连接请求
		if(FD_ISSET(server_fd,&read_fds)) {
			//接受新的连接请求
			client_fd = accept(server_fd,(struct sockaddr *)&client_addr,&addr_len);
			if(0 > client_fd) {
				perror("accept error\r\n");
				return -1;
			}

		struct client_info *new_client = (struct client_info *)malloc(sizeof(*new_client));
		new_client->fd = client_fd;
		new_client->name[0] = '\0';//名字未设置
		new_client->psk[0]  = '\0';//密码未设置

			//添加到管理链表
			if(0 > client_add_manager(new_client,manager)) {
				perror("client_info add to manager failed\r\n");
				return -2;
			}
			//发送欢迎消息
			char welcome[] = "It `s a simple demo ,Just to chat!!!\r\n";
			send(client_fd,welcome,strlen(welcome),0);
		}
		
		
		struct client_info *temp_client;
                struct client_info *temp_n_client;


		//处理客户端消息
		//遍历客户端信息链表
		manager_for_each_client_safe(client,n_client,manager->head,entry) {
			//如果客户端存在活动,处理
			if(client->fd > 0 && FD_ISSET(client->fd,&read_fds)) {
				memset(buffer,0,BUFFER_SIZE);
				//读取客户端信息
				int bytes = read(client->fd,buffer,BUFFER_SIZE - 1);
				if(0 >= bytes) {
					//读取有问题
					if(0 == bytes) {
						//客户端正常断开
						printf("%s 客户已断开连接!\r\n",client->name);
					} else {
						perror("have a problem in reading\r\n");
						printf("%s 客户异常断开!\r\n",client->name);
					}
					close(client->fd);
					client->fd = 0;
					
					//从链表中删除
					del_client(client);

					//通知其他客户端
					manager_for_each_client_safe(temp_client,temp_n_client,manager->head,entry) {
						char leave_info[64];
						snprintf(leave_info,sizeof(leave_info),"用户 %s 已离开!\r\n",client->name);
						if(temp_client != client) {
							send(temp_client->fd, leave_info, strlen(leave_info), 0);
						}
					}
		} else {
				buffer[bytes] = '\0';//末尾补0
				if(client->name[0] == '\0') {
					// 第一条消息 → 当作用户名，去掉末尾\r\n
					int len = bytes;
					while(len > 0 && (buffer[len-1] == '\r' || buffer[len-1] == '\n')) len--;
					buffer[len] = '\0';
					strncpy(client->name, buffer, sizeof(client->name)-1);
					client->name[sizeof(client->name)-1] = '\0';
					printf("用户 [%s] 已连接，等待密码...\r\n", client->name);
				} else if(client->psk[0] == '\0') {
					// 第二条消息 → 当作用户密码，去掉末尾\r\n
					int len = bytes;
					while(len > 0 && (buffer[len-1] == '\r' || buffer[len-1] == '\n')) len--;
					buffer[len] = '\0';
					strncpy(client->psk, buffer, sizeof(client->psk)-1);
					client->psk[sizeof(client->psk)-1] = '\0';
					printf("用户 [%s] 登录成功\r\n", client->name);

					// 广播新用户加入
					char join_info[96];
					snprintf(join_info, sizeof(join_info), "用户 %s 加入了聊天室\r\n", client->name);
					manager_for_each_client_safe(temp_client,temp_n_client,manager->head,entry) {
						if(temp_client != client) {
							send(temp_client->fd, join_info, strlen(join_info), 0);
						}
					}
				} else {
					// 登录后的普通聊天消息 → 广播
					printf("收到消息: %s\r\n",buffer);
					manager_for_each_client_safe(temp_client,temp_n_client,manager->head,entry) {
						if(temp_client != client) {//转发给不同客户
							send(temp_client->fd,buffer,strlen(buffer),0);
						}
					}
				}
			}

			}
		}
	}
	
	close(server_fd);
	return 0;
}




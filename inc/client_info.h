#ifndef __CLIENT_INFO_H__
#define __CLIENT_INFO_H__

#include "list.h"

#define PSK_SIZE 32

//客户信息
struct client_info {
	char name[50];
	char psk[PSK_SIZE];
	int fd;
	struct list_entry entry;//侵入式链表
};
//管理链表
struct manager_device {
	struct list_entry *head;
};




//初始化管理者
struct manager_device *manager_init(void);
//摧毁管理者
void manager_destroy(struct manager_device *manager);
//判读管理链表是否为空
bool manager_list_is_empty(struct manager_device *manager);
//删除用户
void del_client(struct client_info *client);
//加入管理链表
int client_add_manager(struct client_info *client, struct manager_device *manager);
//安全遍历客户节点
#define manager_for_each_client_safe(pos,n,head,member) \
	list_for_each_entry_safe(pos,n,head,member)



#endif


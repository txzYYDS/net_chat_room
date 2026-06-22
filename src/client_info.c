#include "client_info.h"


//初始化管理者
struct manager_device *manager_init(void) {
	struct manager_device *manager = (struct manager_device *)malloc(sizeof(*manager));
	if(NULL == manager) {
		return NULL;
	}

	manager->head = (struct list_entry *)malloc(sizeof(struct list_entry));
	if(NULL == manager->head) {
		free(manager);
		return NULL;
	}
	INIT_LIST_HEAD(manager->head);

	return manager;
}

//摧毁管理者
void manager_destroy(struct manager_device *manager) {
	free(manager->head);
	manager->head = NULL;
	free(manager);
	manager = NULL;
}

//判读管理链表是否为空
bool manager_list_is_empty(struct manager_device *manager) {
	return list_is_empty(manager->head);
}

//删除用户
void del_client(struct client_info *client) {
	list_del(&client->entry);
}

//加入管理链表
int client_add_manager(struct client_info *client, struct manager_device *manager) {
	struct list_entry *entry = &client->entry;
	struct list_entry *head = manager->head;
	if(NULL == entry || NULL == head) {
		return -1;
	}
	list_add_tail(entry,head);
	return 0;
}



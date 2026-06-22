#include "list.h"


//初始化链表头
void INIT_LIST_HEAD(struct list_entry *list) {
	//内核中这两部分全部为原子操作
	list->prev = list;
	list->next = list;
}



//链表是否为空
bool list_is_empty(struct list_entry *head) {
	return (head->prev == head && head->next == head);
}


int list_is_head(struct list_entry *list, struct list_entry *head) {
        return list == head;
}



//验证添加链表节点的操作是否有效
bool list_add_valid(struct list_entry *new,struct list_entry *prev,struct list_entry *next) {
	if(next->prev == prev && prev->next == next && new != prev && new != next) {
		return true;
	}
	return false;
}


//验证删除链表节点的操作是否有效
bool list_del_entry_valid(struct list_entry *entry) {
	struct list_entry *prev = entry->prev;
	struct list_entry *next = entry->next;
	if(prev->next == entry && next->prev == entry) {
		return true;
	}
	return false;
}


//添加链表节点---基础操作
void list_add_basic(struct list_entry *new,struct list_entry *prev,struct list_entry *next) {
	if(!list_add_valid(new,prev,next)) {
		return;
	}

	prev->next = new;//这里内核里面使用的是原子操作
	new->prev = prev;
	new->next = next;
	next->prev = new;
}

//添加到链表头部
void list_add(struct list_entry *new,struct list_entry *head) {
	list_add_basic(new,head,head->next);
}

//添加到链表尾部
void list_add_tail(struct list_entry *new,struct list_entry *head) {
	list_add_basic(new,head->prev,head);
}

//删除链表节点的基础操作
void list_del_basic(struct list_entry *prev, struct list_entry *next) {
	prev->next = next;//内核这里使用的原子操作
	next->prev = prev;
}


//删除链表节点
void list_del_entry(struct list_entry *entry) {
	//检查删除操作是否合法
	if(!list_del_entry_valid(entry)) {
		return;
	}

	list_del_basic(entry->prev,entry->next);
}

//链表删除节点
void list_del(struct list_entry *entry) {
	list_del_entry(entry);

	//毒化指针
	entry->next = (struct list_entry *)LIST_POSITION1;
	entry->prev = (struct list_entry *)LIST_POSITION2;	
}


//节点替换操作
void list_replace(struct list_entry *old,struct list_entry *new) {
	new->next = old->next;
	new->prev = old->prev;
	new->next->prev = new;
	new->prev->next = new;
}



//替换并初始化,使用新节点替换老节点，并初始化老节点
void list_replace_init(struct list_entry *old,struct list_entry *new) {
	list_replace(old,new);
	INIT_LIST_HEAD(old);
}



//交换链表中两个节点的位置
void list_swap(struct list_entry *entry1,struct list_entry *entry2) {
	struct list_entry *pos = entry2->prev;
	list_del(entry2);//从链表中删掉entry2节点
	list_replace(entry1,entry2);//使用entry2节点替换掉entry1节点
	if(pos == entry1) {//刚好entry2前面一个节点是entry1
		pos = entry2;
	}

	list_add(entry1,pos);//将entry1节点添加到原entry2节点的位置
}


//删除节点并初始化
void list_del_init(struct list_entry *entry) {
	list_del_entry(entry);//删除节点
	INIT_LIST_HEAD(entry);//初始化为新的链表头
}


//移动链表节点到链表头部
void list_move(struct list_entry *entry,struct list_entry *head) {
	list_del_entry(entry);//从链表中删除节点
	list_add(entry,head);//将节点添加到链表头
}

//移动链表节点到链表尾部
void list_move_tail(struct list_entry *entry,struct list_entry *head) {
	list_del_entry(entry);
	list_add_tail(entry,head);
}




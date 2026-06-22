#ifndef __LIST_H__
#define __LIST_H__

#include "common.h"

#define LIST_POSITION1 0x1000
#define LIST_POSITION2 0x2000
/*
 *若不想自己实现offsetof可以使用如下
 *offsetof(type,member)
 * */
//侵入式链表
#define container_of(ptr,type,member) \
        ((type *)((char *)ptr - (char *)(&(((type *)0)->member))))

struct list_entry {
        struct list_entry *prev;
        struct list_entry *next;
};


//初始化链表头
#define LIST_INIT_HEAD(name) {&(name),&(name)}
/*
 *example:
 *      struct list_entry head = LIST_INIT_HEAD(head);
 * */


//获取被侵入链表节点的结构体地址
#define list_entry(ptr,type,member) \
        container_of(ptr,type,member)

//获取第一个节点地址
#define list_first_entry(ptr,type,member) \
        list_entry((ptr)->next,type,member)

//获取最后一个节点地址
#define list_last_entry(ptr,type,member) \
        list_entry((ptr)->prev,type,member)

#define list_first_entry_or_null(ptr,type,member) ({ \
        struct list_entry *head__ = ptr; \
        struct list_entry *pos__ = head__->next; \
        pos__ != head__ ? list_entry(pos__,type,member) : NULL; \
})

int list_is_head(struct list_entry *list, struct list_entry *head);

#define list_next_entry(pos, member) \
        list_entry((pos)->member.next, typeof(*(pos)), member)

#define list_entry_is_head(pos, head, member)                           \
        list_is_head(&pos->member, (head))


#define list_for_each_entry_safe(pos, n, head, member)                  \
        for (pos = list_first_entry(head, typeof(*pos), member),        \
                n = list_next_entry(pos, member);                       \
             !list_entry_is_head(pos, head, member);                    \
             pos = n, n = list_next_entry(n, member))



//初始化链表头
void INIT_LIST_HEAD(struct list_entry *list);
//链表是否为空
bool list_is_empty(struct list_entry *head);
//验证添加链表节点的操作是否有效
bool list_add_valid(struct list_entry *new,struct list_entry *prev,struct list_entry *next);
//验证删除链表节点的操作是否有效
bool list_del_entry_valid(struct list_entry *entry);
//添加链表节点---基础操作
void list_add_basic(struct list_entry *new,struct list_entry *prev,struct list_entry *next);
//添加到链表头部
void list_add(struct list_entry *new,struct list_entry *head);
//添加到链表尾部
void list_add_tail(struct list_entry *new,struct list_entry *head);
//删除链表节点的基础操作
void list_del_basic(struct list_entry *prev, struct list_entry *next);
//删除链表节点
void list_del_entry(struct list_entry *entry);
//链表删除节点
void list_del(struct list_entry *entry);
//节点替换操作
void list_replace(struct list_entry *old,struct list_entry *new);
//替换并初始化,使用新节点替换老节点，并初始化老节点
void list_replace_init(struct list_entry *old,struct list_entry *new);
//交换链表中两个节点的位置
void list_swap(struct list_entry *entry1,struct list_entry *entry2);
//删除节点并初始化
void list_del_init(struct list_entry *entry);
//移动链表节点到链表头部
void list_move(struct list_entry *entry,struct list_entry *head);
//移动链表节点到链表尾部
void list_move_tail(struct list_entry *entry,struct list_entry *head);



#endif

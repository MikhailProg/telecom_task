#ifndef LIST_H
#define LIST_H

struct list {
	struct list	*prev;
	struct list 	*next;
};

inline static void list_prepend(struct list **head, struct list *list)
{
	list->prev = NULL;
	list->next = *head;
	if (*head)
		(*head)->prev = list;
	*head = list;	
}

inline static void list_remove(struct list **head, struct list *list)
{
	if (list->prev != NULL)
		list->prev->next = list->next;
	if (list->next != NULL)
		list->next->prev = list->prev;
	
	/* List is a head. */
	if (list->prev == NULL) {
		assert(list == *head);
		*head = list->next;
	}

	list->prev = list->next = NULL;
}

inline static void
list_foreach(struct list *head,
	     void (*callback)(struct list *, void *), void *opaque)
{
	struct list *iter = head, *next;

	while (iter != NULL) {
		next = iter->next;
		callback(iter, opaque);
		iter = next;
	}
}

#endif

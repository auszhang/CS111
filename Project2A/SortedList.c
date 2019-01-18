/*
 * NAME: Austin Zhang
 * EMAIL: aus.zhang@gmail.com
 * ID: 604736503
 */

#include "SortedList.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sched.h>

/**
 * SortedList (and SortedListElement)
 *
 *	A doubly linked list, kept sorted by a specified key.
 *	This structure is used for a list head, and each element
 *	of the list begins with this structure.
 *
 *	The list head is in the list, and an empty list contains
 *	only a list head.  The next pointer in the head points at
 *      the first (lowest valued) elment in the list.  The prev
 *      pointer in the list head points at the last (highest valued)
 *      element in the list.  Thus, the list is circular.
 *
 *      The list head is also recognizable by its NULL key pointer.
 *
 * NOTE: This header file is an interface specification, and you
 *       are not allowed to make any changes to it.
 */

int opt_yield = 0;

/**
 * SortedList_insert ... insert an element into a sorted list
 *
 *	The specified element will be inserted in to
 *	the specified list, which will be kept sorted
 *	in ascending order based on associated keys
 *
 * @param SortedList_t *list ... header for the list
 * @param SortedListElement_t *element ... element to be added to the list
 */
void SortedList_insert(SortedList_t *list, SortedListElement_t *element) {

	SortedListElement_t* temp = list->next;
	SortedListElement_t* current = list;

	if (!list || !element) {
        return;
	}

	// edge case
	if (list->next == NULL) {
    	if (opt_yield & INSERT_YIELD) {
      		sched_yield();
    	}

		list->next = element;
		element->prev = list;
		element->next = NULL;
        return;
	}

	// find out where the element belongs
   	while (temp != NULL && strcmp(element->key, temp->key) >= 0 ){
		current = temp;
		temp = temp->next;
    } 

    // schedule yield if opt_yield & INSERT_YIELD
	if (opt_yield & INSERT_YIELD) {
    	sched_yield();
	}
	
	if (temp != NULL) {
		element->next = temp;
		element->prev = current;
		temp->prev = element;
		current->next = element;
   	} else {
   		current->next = element;
		element->prev = current;
		element->next = NULL;
    }
}

/**
 * SortedList_delete ... remove an element from a sorted list
 *
 *	The specified element will be removed from whatever
 *	list it is currently in.
 *
 *	Before doing the deletion, we check to make sure that
 *	next->prev and prev->next both point to this node
 *
 * @param SortedListElement_t *element ... element to be removed
 *
 * @return 0: element deleted successfully, 1: corrtuped prev/next pointers
 *
 */
int SortedList_delete(SortedListElement_t *element) {
	if (element == NULL) {
        return 1;
	}

	/* critical section */
	if (opt_yield & DELETE_YIELD) {
    	sched_yield();
	}

	if (element->next != NULL) {
    	if (element->next->prev == element) {
        	element->next->prev = element->prev;
    	} else {
    		return 1;
    	}
	}

	if (element->prev != NULL) {
    	if (element->prev->next == element) {
       		element->prev->next = element->next;
    	} else {
    		return 1;
    	}
	}

	return 0;

}

/**
 * SortedList_lookup ... search sorted list for a key
 *
 *	The specified list will be searched for an
 *	element with the specified key.
 *
 * @param SortedList_t *list ... header for the list
 * @param const char * key ... the desired key
 *
 * @return pointer to matching element, or NULL if none is found
 */
SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key) {	
	SortedListElement_t* current = list->next;

	if (list == NULL) {
    	return NULL;
	}
	if (opt_yield & LOOKUP_YIELD) {
      	sched_yield();
    }

	while (current != NULL) {
		if (strcmp(key, current->key) == 0) {
        		return current;
    	}
   	 	current = current->next;
	}
	return NULL;
}

/**
 * SortedList_length ... count elements in a sorted list
 *	While enumeratign list, it checks all prev/next pointers
 *
 * @param SortedList_t *list ... header for the list
 *
 * @return int number of elements in list (excluding head)
 *	   -1 if the list is corrupted
 */
int SortedList_length(SortedList_t *list) {
	int length = 0;
	SortedListElement_t* current = list->next;

	if (list == NULL) {
		return -1;
	}
	while (current != NULL) {
    	if (opt_yield & LOOKUP_YIELD) {
      		sched_yield();
    	}

    	current = current->next;
    	length++;
	}
	return length;
}
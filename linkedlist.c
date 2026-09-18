#include <stdio.h>
#include <stdlib.h>

#include "linkedlist.h"

/* Initialisation of the list
 * */
list_ptr list_new(void)
{
  return NULL;
}

/* Add a new cel to a list. 
 *  store the sprite_t to the new cel
 * */
list_ptr list_add(sprite_t sprite, list_ptr liste_sprite)
{
  list_ptr new_cel = (list_ptr)malloc(sizeof(s_list_node_t));
  new_cel->data = sprite;
  new_cel->next = liste_sprite;
  return new_cel;
}

/* Return true if the list is empty
 * */
bool list_is_empty(list_ptr  liste_sprite)
{
  if(liste_sprite == NULL){
    return true;
  } else {
    return false;
  }
}

/* Return the next cel in list or NULL
 * */
list_ptr list_next(list_ptr liste_sprite)
{
  if(liste_sprite == NULL){
    return NULL;
  }
  return liste_sprite->next;
}

/* Search the first cel of the list & 
 *  return the associated sprite 
 * */
sprite_t list_head_sprite(list_ptr liste_sprite)
{
  if(liste_sprite == NULL){
    return NULL;
  }
  return liste_sprite->data;
}

/* Search the last cel of a list 
 *  Remove the cel from the list
 *  Return the associated sprite
 * */
sprite_t list_pop_sprite(list_ptr * liste_sprite)
{
  if(liste_sprite == NULL || *liste_sprite == NULL){
    return NULL;
  }
  list_ptr previous = NULL;
  list_ptr temp = *liste_sprite;
  while(temp->next != NULL){
    previous = temp;
    temp = temp->next;
  }
  sprite_t sprite = temp->data;
  if (previous == NULL)
    *liste_sprite = NULL;
  else
    previous->next = NULL;
  free(temp);
  return sprite;
}

/* Remove the given cel in a list
 * */
void list_remove(list_ptr element, list_ptr *liste_sprite)
{
  if(*liste_sprite == NULL || element == NULL){
    return;
  }
  if(*liste_sprite == element){
    *liste_sprite = element->next;
    free(element);
    return;
  }
  list_ptr temp = *liste_sprite;
  while(temp->next != NULL && temp->next != element){
    temp = temp->next;
  }
  if(temp->next == element){
    temp->next = element->next;
    free(element);
  }
}

/* Wipe out a list. 
 *  Don't forget to sprite_free() for each sprite
 * */
void list_free(list_ptr list)
{
    while(list != NULL){
  list_ptr next = list->next;
      sprite_free(list->data);
      free(list);
      list = next;
    }
}

/* Return the length of a list
 * */
int list_length(list_ptr liste_sprite)
{
  int count = 0;
  while(liste_sprite != NULL){
    count++;
    liste_sprite = liste_sprite->next;
  }
  return count;
}

/* Reverse the order of a list
 * */
void list_reverse(list_ptr * list_sprite)
{
  list_ptr prev = NULL;
  list_ptr current = *list_sprite;
  list_ptr next = NULL;
  while(current != NULL){
    next = current->next;
    current->next = prev;
    prev = current;
    current = next;
  }
  *list_sprite = prev;
}

/* Copy a list to another one. 
 *  Return the new list
 * */
list_ptr list_clone(list_ptr list_sprite)
{
  list_ptr new_list = NULL;
  list_ptr temp = list_sprite;
  while(temp != NULL){
    new_list = list_add(temp->data, new_list);
    temp = temp->next;
  }
  return new_list;
}

/******************************************************
 * Copyright Grégory Mounié 2018                      *
 * This code is distributed under the GLPv3+ licence. *
 * Ce code est distribué sous la licence GPLv3+.      *
 ******************************************************/

#include <assert.h>
#include "mem.h"
#include "mem_internals.h"

void *emalloc_small(unsigned long size){
  /* Si le chunkpool est vide */
  if(arena.chunkpool == NULL){
    unsigned long size = mem_realloc_small(); //On crée un grand bloc
    void **p = (void**)arena.chunkpool;
    /* Tous les 96 octets on "découpe" le bloc en chaînant les bouts entre eux */
    for(int i = 0; i < size; i+=CHUNKSIZE){
      *p = (char*)p + 96;
      p = (void**)(*p);
    }
  }

  /* A cette étape on a un chunkpool forcément non-vide */
  void **c = (void **)arena.chunkpool; //On pointe sur le premier bloc
  arena.chunkpool = *c; //On détache le bloc en faisant pointer le début du chunkpool sur le bloc d'après
  c = (void **)mark_memarea_and_get_user_ptr(c,CHUNKSIZE,SMALL_KIND); //Marquage
  return (void *)c;
}

void efree_small(Alloc a) {
  void **c = (void **)a.ptr; //On prend la valeur du pointeur dans a
  *c = arena.chunkpool; //On fixe le "next" au premier bloc du chunkpool
  arena.chunkpool = c; //Le bloc de a devient le premier bloc du chunkpool
}

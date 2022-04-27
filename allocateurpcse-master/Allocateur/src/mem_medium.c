/******************************************************
 * Copyright Grégory Mounié 2018                      *
 * This code is distributed under the GLPv3+ licence. *
 * Ce code est distribué sous la licence GPLv3+.      *
 ******************************************************/

#include <stdint.h>
#include <assert.h>
#include "mem.h"
#include "mem_internals.h"
#include <stdbool.h>

unsigned int puiss2(unsigned long size) {
    unsigned int p=0;
    size = size -1; // allocation start in 0
    while(size) {  // get the largest bit
	p++;
	size >>= 1;
    }
    if (size > (1 << p))
	p++;
    return p;
}

/* Retourne 2 puissance puiss */
unsigned long calc_puiss2(unsigned int puiss){
  unsigned long size = 1;
  for(int i = 1; i <= puiss; i++){
    size = size*2;
  }
  return size;
}


/* Découpe le bloc de l'indice indice_max pour placer les 2 moitiés dans la case
d'indice indice_max-1, et ce récursivement jusqu'à la case indice+1 (on ne découpe
pas dans la case indice) */
void decoupe(int indice_min,int indice_max) {
  if(indice_min != indice_max){ //On découpe seulement si on a pas atteint la case indice

    void **p = arena.TZL[indice_max]; //Bloc à découper
    arena.TZL[indice_max] = *p; //On décroche le bloc

    /* On fait pointer le "next" du bloc sur sa moitié (coupe en 2) */
    *p = (void *)((unsigned long)p ^ calc_puiss2(indice_max-1));

    /* On place les deux moitiés en tête de la liste de la case d'en-dessous */
    void **c = *p;
    *c = arena.TZL[indice_max-1]; //Le "next" de la 2e moitié du bloc pointe sur la tête de la case
    arena.TZL[indice_max-1] = p; //On place le bloc en tête de liste

    decoupe(indice_min,indice_max-1); //Rappel de la fonction pour la case d'en-dessous
  }
}

void *emalloc_medium(unsigned long size) {
    assert(size < LARGEALLOC);
    assert(size > SMALLALLOC);
    int indice = puiss2(size+32);
    int indice_fixe = indice; // On sauvegarde la valeur d'indice, cette dernière étant amenée à changer
    bool bloc_trouve = false; // true si un bloc de taille suffisante a été trouvé

    /* On regarde les cases de TZL pour voir si un bloc de taille suffisante existe */
    while(bloc_trouve == false && indice < (FIRST_ALLOC_MEDIUM_EXPOSANT + arena.medium_next_exponant)){
      if(arena.TZL[indice] != NULL){
        bloc_trouve = true;
      }
      else{
        indice ++;
      }
    }

    /* Si aucun bloc trouvé, on appelle à en créer un dans la dernière case de TZL */
    if(bloc_trouve == false){
      mem_realloc_medium();
    }

    /* Si l'indice du bloc existant le plus petit est plus grand que l'indice idéal, on découpe de manière récursive */
    if(indice != indice_fixe){
      decoupe(indice_fixe,indice);
    }

    /* A ce stade on a un bloc de taille idéale au bon indice */
    /* On pointe le bloc */
    void **p = arena.TZL[indice_fixe];
    /* On fait pointer le début de la liste sur l'élément suivant */
    arena.TZL[indice_fixe] = *p;
    /* On marque le bloc */
    return mark_memarea_and_get_user_ptr(p,calc_puiss2(indice_fixe),MEDIUM_KIND);
}

void efree_medium(Alloc a) {
    int indice = puiss2(a.size);

    /* Si la case est vide on place le bloc dedans */
    if(arena.TZL[indice] == NULL){
      void **insert = a.ptr;
      *insert = arena.TZL[indice];
      arena.TZL[indice] = insert;
    }

    /* Sinon on va parcourir la case pour voir si le buddy du bloc s'y trouve */
    else{
      void *buddy = (void *)((unsigned long)a.ptr ^ a.size);
      bool trouve = false; // = true si on trouve le bloc
      void **p = arena.TZL[indice];

      /* Si le premier bloc s'avère être le buddy */
      if(p == buddy){
        trouve = true;
        arena.TZL[indice] = *p;
        a.size = a.size*2;
        /* Si l'adresse du pointeur est supérieure au buddy, le début du bloc de size*2 est le buddy */
        if(a.ptr > buddy){
          a.ptr = buddy;
        }
        /* On rappelle la fonction pour tenter de placer le nouveau bloc de size*2 dans la case d'au-dessus */
        efree_medium(a);
      }

      /* Sinon on parcours la case */
      else{
        while(*p != NULL && trouve == false){
          if(*p == buddy){
            trouve = true;
            void **q = *p;
            *p = (void **)(*q);
            a.size = a.size*2; //On double la taille dans a
            /* Si l'adresse du pointeur est supérieure au buddy, le début du bloc de size*2 est le buddy */
            if(a.ptr > buddy){
              a.ptr = buddy;
            }
            /* On rappelle la fonction pour tenter de placer le nouveau bloc de size*2 dans la case d'au-dessus */
            efree_medium(a);
          }
          else{
            p = *p; // Parcours de la liste de blocs
          }
        }
      }

      /* Si le buddy n'est pas présent on ajoute le bloc en début de liste */
      if(trouve == false){
        void **insert = a.ptr;
        *insert = arena.TZL[indice];
        arena.TZL[indice] = insert;
      }
    }
}

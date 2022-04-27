/******************************************************
 * Copyright Grégory Mounié 2018                      *
 * This code is distributed under the GLPv3+ licence. *
 * Ce code est distribué sous la licence GPLv3+.      *
 ******************************************************/

#include <sys/mman.h>
#include <assert.h>
#include <stdint.h>
#include "mem.h"
#include "mem_internals.h"

unsigned long knuth_mmix_one_round(unsigned long in){
    return in * 6364136223846793005UL % 1442695040888963407UL;
}

/* Calcul de la valeur magique à partir du pointeur de bloc et du type d'allocation */
unsigned long calc_magic(unsigned long *p, MemKind k){
    unsigned long mmix_62 = knuth_mmix_one_round((unsigned long)p) & ~(0b11UL); //Génération des 62 bits de poids fort mmix
    return mmix_62|k; //On fusionne les 62 bits de poids fort et k pour créer le nombre magique
}

void *mark_memarea_and_get_user_ptr(void *ptr, unsigned long size, MemKind k){
    unsigned long taille = size - 4*sizeof(unsigned long); //Taille dans laquelle peut écrire l'utilisateur
    unsigned long *p = ptr;
    unsigned long magic = calc_magic(p,k);
    *p = size; //On écrit size dans les 8 premiers octets du bloc

    /* On parcours 8 octets pour écrire magic */
    p += 1;
    *p = magic;

    /* On parcours 8 octets + taille pour écrire magic */
    char *q = (char *)p + sizeof(unsigned long) + taille;
    p = (unsigned long *)q;
    *p = magic;

    /* On parcours 8 octets pour écrire size */
    p += 1;
    *p = size;

    /* On place le pointeur 16 octets après le début du bloc mémoire (début de la zone utilisateur) */
    ptr += 2*sizeof(unsigned long);
    return ptr;
}

Alloc mark_check_and_get_alloc(void *ptr){
    unsigned long *p = ptr;

    /* On va voir 16 octets avant ptr pour lire la première occurrence de size */
    p -= 2;
    unsigned long size = *p;

    unsigned long taille = size - 4*sizeof(unsigned long); // On récupère la taille réservée à l'utilisateur

    /* On recule de 8 octets pour lire la première occurrence de magic */
    p += 1;
    unsigned long magic = *p;

    /* avance de 8 + taille octets si magic est écrit correctement dans les 2 cases */
    char *q = (char *)p + sizeof(unsigned long) + taille;
    p = (unsigned long *)q;
    unsigned long magic_verif = *p;
    assert(magic == magic_verif);

    /* On avance de 8 octets si size est écrit correctement dans les 2 cases */
    p += 1;
    unsigned long size_verif = *p;
    assert(size == size_verif);

    /* On lit le type de taille (small, medium, etc..) */
    unsigned long type = magic & 0b11UL;
    MemKind k = type;

    ptr -= 2*sizeof(unsigned long); // On replace le pointeur en tout début de bloc

    assert(calc_magic((unsigned long *)ptr,k) == magic); // Vérification valeur magique

    Alloc a = {ptr,k,size};
    return a;
}

unsigned long mem_realloc_small() {
    assert(arena.chunkpool == 0);
    unsigned long size = (FIRST_ALLOC_SMALL << arena.small_next_exponant);
    arena.chunkpool = mmap(0,
			   size,
			   PROT_READ | PROT_WRITE | PROT_EXEC,
			   MAP_PRIVATE | MAP_ANONYMOUS,
			   -1,
			   0);
    if (arena.chunkpool == MAP_FAILED) handle_fatalError("small realloc");
    arena.small_next_exponant++;
    return size;
}

unsigned long mem_realloc_medium() {
    uint32_t indice = FIRST_ALLOC_MEDIUM_EXPOSANT + arena.medium_next_exponant;
    assert(arena.TZL[indice] == 0);
    unsigned long size = (FIRST_ALLOC_MEDIUM << arena.medium_next_exponant);
    assert( size == (1 << indice));
    arena.TZL[indice] = mmap(0,
			     size*2, // twice the size to allign
			     PROT_READ | PROT_WRITE | PROT_EXEC,
			     MAP_PRIVATE | MAP_ANONYMOUS,
			     -1,
			     0);
    if (arena.TZL[indice] == MAP_FAILED) handle_fatalError("medium realloc");
    // align allocation to a multiple of the size
    // for buddy algo
    arena.TZL[indice] += (size - (((intptr_t)arena.TZL[indice]) % size));
    arena.medium_next_exponant++;
    return size; // lie on allocation size, but never free
}

// used for test in buddy algo
unsigned int nb_TZL_entries() {
    int nb = 0;

    for(int i=0; i < TZL_SIZE; i++)
	if ( arena.TZL[i] )
	    nb ++;

    return nb;
}

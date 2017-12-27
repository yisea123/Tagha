#include <stdlib.h>
#include <assert.h>
#include <iso646.h>		// and, or, not
#include <string.h>
#include <stdio.h>
#include "tagha.h"


uint32_t gethash32(const char *strKey)
{
	const uint8_t *us;
	uint32_t h = 0;
	for( us=(const uint8_t *)strKey ; *us ; us++ )
		h = 37 * h + *us;
	return h;
}

uint64_t gethash64(const char *strKey)
{
	uint64_t h = 0;
	for( char *us = (char *)strKey ; *us ; us++ )
		h = h * 147 + *us;

	return h;
}

uint32_t int32hash(uint32_t x)
{
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = (x >> 16) ^ x;
	return x;
}

uint64_t int64hash(uint64_t x)
{
	x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
	x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
	x = x ^ (x >> 31);
	return x;
}


struct Hashmap *Map_New(void)
{
	return calloc(1, sizeof(struct Hashmap));
}

void Map_Init(struct Hashmap *map)
{
	if( !map )
		return;
	
	map->table = NULL;
	map->size = map->count = 0;
}

void Map_Free(struct Hashmap *map)
{
	if( !map || !map->table )
		return;
	
	/*
	 * If the struct Hashmapionary pointer is not "const", then
	 * you have to make two traversing struct KeyNode pointers
	 * or else you'll get a nice little segfault ;)
	 * not sure why but whatever makes the code work I guess.
	*/
	struct KeyNode
		*kv = NULL,
		*next = NULL
	;
	for( uint32_t i=0 ; i<map->size ; i++ ) {
		for( kv = map->table[i] ; kv ; kv = next ) {
			next = kv->pNext;
			kv->pData = 0;
			kv->strKey = NULL;
			free(kv), kv = NULL;
		}
	}
	if( map->table )
		free(map->table);
	Map_Init(map);
}

bool Map_Insert(struct Hashmap *restrict map, const char *restrict strKey, const uint64_t pData)
{
	if( !map ) {
		printf("**** Hashmap Error **** Map_Insert::map is NULL\n");
		return false;
	}
	if( map->size == 0 ) {
		map->size = 8;
		map->table = calloc(map->size, sizeof(struct KeyNode));
		if( !map->table ) {
			printf("**** Memory Allocation Error **** Map_Insert::map->table is NULL\n");
			map->size = 0;
			return false;
		}
	}
	else if( map->count >= map->size ) {
		Map_Rehash(map);
		//printf("**** Rehashed struct Hashmapionary ****\n");
		//printf("**** struct Hashmapionary Size is now %llu ****\n", map->size);
	}
	else if( Map_HasKey(map, strKey) ) {
		printf("Map_Insert::map already has entry!\n");
		return false;
	}
	
	struct KeyNode *node = calloc(1, sizeof(struct KeyNode) );
	if( !node ) {
		printf("**** Memory Allocation Error **** Map_Insert::node is NULL\n");
		return false;
	}
	node->strKey = strKey;
	node->pData = pData;
	
	uint32_t hash = gethash32(strKey) % map->size;
	node->pNext = map->table[hash];
	map->table[hash] = node;
	++map->count;
	return true;
}

uint64_t Map_Get(const struct Hashmap *restrict map, const char *restrict strKey)
{
	if( !map || !map->table )
		return 0;
	/*
	 * if struct Hashmapionary pointer is const, you only
	 * need to use one traversing struct KeyNode
	 * pointer without worrying of a segfault
	*/
	struct KeyNode *kv;
	uint32_t hash = gethash32(strKey) % map->size;
	for( kv = map->table[hash] ; kv ; kv = kv->pNext )
		if( !strcmp(kv->strKey, strKey) )
			return kv->pData;
	return 0;
}

void Map_Delete(struct Hashmap *restrict map, const char *restrict strKey)
{
	if( !map || !Map_HasKey(map, strKey) )
		return;
	
	uint32_t hash = gethash32(strKey) % map->size;
	struct KeyNode
		*kv = NULL,
		*next = NULL
	;
	for( kv = map->table[hash] ; kv ; kv = next ) {
		next = kv->pNext;
		if( !strcmp(kv->strKey, strKey) ) {
			map->table[hash] = kv->pNext;
			free(kv), kv = NULL;
			map->count--;
		}
	}
}

bool Map_HasKey(const struct Hashmap *restrict map, const char *restrict strKey)
{
	if( !map || !map->table )
		return false;
	
	struct KeyNode *prev;
	uint32_t hash = gethash32(strKey) % map->size;
	for( prev = map->table[hash] ; prev ; prev = prev->pNext )
		if( !strcmp(prev->strKey, strKey) )
			return true;
	
	return false;
}

uint64_t Map_Len(const struct Hashmap *map)
{
	if( !map )
		return 0L;
	return map->count;
}

// Rehashing increases struct Hashmapionary size by a factor of 2
void Map_Rehash(struct Hashmap *map)
{
	if( !map || !map->table )
		return;
	
	uint64_t old_size = map->size;
	map->size <<= 1;
	map->count = 0;
	
	struct KeyNode **curr, **temp;
	temp = calloc(map->size, sizeof(struct KeyNode));
	if( !temp ) {
		printf("**** Memory Allocation Error **** Map_Insert::temp is NULL\n");
		map->size = 0;
		return;
	}
	
	curr = map->table;
	map->table = temp;
	
	struct KeyNode
		*kv = NULL,
		*next = NULL
	;
	for( uint32_t i=0 ; i<old_size ; ++i ) {
		if( !curr[i] )
			continue;
		for( kv = curr[i] ; kv ; kv = next ) {
			next = kv->pNext;
			Map_Insert(map, kv->strKey, kv->pData);
			// free the inner nodes since they'll be re-hashed
			kv->strKey=NULL, kv->pData=0;
			free(kv), kv = NULL;
			//printf("**** Rehashed Entry ****\n");
		}
	}
	if( curr )
		free(curr);
	curr = NULL;
}

const char *Map_GetKey(const struct Hashmap *restrict map, const char *restrict strKey)
{
	if( !map || !map->table )
		return NULL;
	
	struct KeyNode *prev;
	uint32_t hash = gethash32(strKey) % map->size;
	for( prev = map->table[hash] ; prev ; prev = prev->pNext )
		if( !strcmp(prev->strKey, strKey) )
			return prev->strKey;
	
	return NULL;
}



/*
bool Map_Insert_int(struct Hashmap *restrict map, const uint64_t key, void *restrict pData)
{
	if( !map )
		return false;
	
	if( map->size == 0 ) {
		map->size = 8;
		map->table = calloc(map->size, sizeof(struct KeyNode));
		
		if( !map->table ) {
			printf("**** Memory Allocation Error **** Map_Insert_int::map->table is NULL\n");
			map->size = 0;
			return false;
		}
	}
	else if( map->count >= map->size ) {
		Map_Rehash(map);
		//printf("**** Rehashed struct Hashmapionary ****\n");
		//printf("**** struct Hashmapionary Size is now %llu ****\n", map->size);
	}
	else if( Map_HasKey_int(map, key) ) {
		printf("Map_Insert_int::map already has entry!\n");
		return false;
	}
	
	struct KeyNode *node = calloc(1, sizeof(struct KeyNode) );
	if( !node ) {
		printf("**** Memory Allocation Error **** Map_Insert_int::node is NULL\n");
		return false;
	}
	node->i64Key = key;
	node->pData = pData;
	
	uint64_t hash = int64hash(key) % map->size;
	node->pNext = map->table[hash];
	map->table[hash] = node;
	++map->count;
	return true;
}

void *Map_Get_int(const struct Hashmap *map, const uint64_t key)
{
	if( !map )
		return NULL;
	else if( !map->table )
		return NULL;
	
	struct KeyNode *kv;
	uint64_t hash = int64hash(key) % map->size;
	for( kv = map->table[hash] ; kv ; kv = kv->pNext )
		if( kv->i64Key==key )
			return kv->pData;
	
	return NULL;
}

void Map_Delete_int(struct Hashmap *map, const uint64_t key)
{
	if( !map )
		return;
	
	if( !Map_HasKey_int(map, key) )
		return;
	
	uint64_t hash = int64hash(key) % map->size;
	struct KeyNode
		*kv = NULL,
		*next = NULL
	;
	for( kv = map->table[hash] ; kv ; kv = next ) {
		next = kv->pNext;
		if( kv->i64Key==key ) {
			map->table[hash] = kv->pNext;
			free(kv), kv = NULL;
			map->count--;
		}
	}
}
bool Map_HasKey_int(const struct Hashmap *map, const uint64_t key)
{
	if( !map )
		return false;
	
	struct KeyNode *prev;
	uint64_t hash = int64hash(key) % map->size;
	for( prev = map->table[hash] ; prev ; prev = prev->pNext )
		if( prev->i64Key==key )
			return true;
	
	return false;
}

void Map_Rehash_int(struct Hashmap *map)
{
	if( !map )
		return;
	
	uint32_t old_size = map->size;
	map->size <<= 1;
	map->count = 0;
	
	struct KeyNode **curr, **temp;
	temp = calloc(map->size, sizeof(struct KeyNode));
	if( !temp ) {
		printf("**** Memory Allocation Error **** Map_Insert::temp is NULL\n");
		map->size = 0;
		return;
	}
	
	curr = map->table;
	map->table = temp;
	
	struct KeyNode
		*kv = NULL,
		*next = NULL
	;
	for( uint32_t i=0 ; i<old_size ; ++i ) {
		if( !curr[i] )
			continue;
		for( kv = curr[i] ; kv ; kv = next ) {
			next = kv->pNext;
			Map_Insert_int(map, kv->i64Key, kv->pData);
			// free the inner nodes since they'll be re-hashed
			free(kv), kv = NULL;
			//printf("**** Rehashed Entry ****\n");
		}
	}
	if( curr )
		free(curr);
	curr = NULL;
}
*/



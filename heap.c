#include "heap.h"
#include<stdio.h>
#include "custom_unistd.h"
#include "tested_declarations.h"
#include "rdebug.h"
#define FENCE_LENGTH 16
struct memory_manager_t memory_manager={NULL, 0, NULL};
const uint8_t FENCE[16] = {0xAA,0xAB,0xAC,0xAD,0xAA,0xAB,0xAC,0xAD,0xAA,0xAB,0xAC,0xAD,0xAA,0xAB,0xAC,0xAD};
int heap_setup(void)
{
    if(memory_manager.memory_start!=NULL)
    {
        return -1;
    }
    memory_manager.memory_start = custom_sbrk(0);
    if(memory_manager.memory_start==(void *)-1)
    {
        memory_manager.memory_start=NULL;
        return -1;
    }
    memory_manager.memory_size=0;
    memory_manager.first_memory_chunk=NULL;
    return 0;
}
void* heap_malloc(size_t size)
{
    if(size == 0) {
        return NULL;
    }
    if(memory_manager.first_memory_chunk == NULL)
    {
        void *request = custom_sbrk(size + sizeof(struct memory_chunk_t)+FENCE_LENGTH*2);
        memory_manager.memory_size+=size + sizeof(struct memory_chunk_t)+FENCE_LENGTH*2;
        if(request==(void *)-1)
        {
            return NULL;
        }
        struct memory_chunk_t * chunk=(struct memory_chunk_t *)request;
        chunk->next=NULL;
        chunk->prev=NULL;
        chunk->free=0;
        chunk->size=size;
        chunk->checksum= calculate_checksum(chunk);
        (memory_manager.first_memory_chunk)=chunk;
        uint8_t *fence_before= (uint8_t *)memory_manager.memory_start + sizeof(struct memory_chunk_t);
        for(int i=0;i<16;i++) {
            *(fence_before+i)=*(FENCE+i);
        }
        uint8_t *fence_after=fence_before+size+FENCE_LENGTH;
        for(int i=0;i<16;i++) {
            *(fence_after+i)=*(FENCE+i);
        }
        return (uint8_t *)memory_manager.memory_start+sizeof(struct memory_chunk_t)+FENCE_LENGTH;
    }
    else
    {
        struct memory_chunk_t * chunk=memory_manager.first_memory_chunk;
        while(1) {
            if(chunk->free==1) {
                if(chunk->size >= size+FENCE_LENGTH*2) {
                    chunk->size=size;
                    chunk->free=0;
                    chunk->checksum= calculate_checksum(chunk);
                    uint8_t *fence_before= (uint8_t *)chunk + sizeof(struct memory_chunk_t);
                    for(int i=0;i<16;i++) {
                        *(fence_before+i)=*(FENCE+i);
                    }
                    uint8_t *fence_after=fence_before+size+FENCE_LENGTH;
                    for(int i=0;i<16;i++) {
                        *(fence_after+i)=*(FENCE+i);
                    }
                    return (uint8_t *)chunk+sizeof(struct memory_chunk_t)+FENCE_LENGTH;
                }
            }
            if(chunk->next==NULL) {
                break;
            }
            chunk=chunk->next;
        }
        struct memory_chunk_t * new_chunk = custom_sbrk(size + sizeof(struct memory_chunk_t)+FENCE_LENGTH*2);
        if(new_chunk==(void *)-1)
        {
            return NULL;
        }
        memory_manager.memory_size+=size + sizeof(struct memory_chunk_t)+FENCE_LENGTH*2;
        new_chunk->free=0;
        new_chunk->size=size;
        new_chunk->next=NULL;
        new_chunk->prev=chunk;
        chunk->next = new_chunk;
        chunk->checksum= calculate_checksum(chunk);
        new_chunk->checksum= calculate_checksum(new_chunk);
        uint8_t *fence_before= (uint8_t *)new_chunk + sizeof(struct memory_chunk_t);
        for(int i=0;i<16;i++) {
            *(fence_before+i)=*(FENCE+i);
        }
        uint8_t *fence_after=fence_before+size+FENCE_LENGTH;
        for(int i=0;i<16;i++) {
            *(fence_after+i)=*(FENCE+i);

        }
        return (uint8_t *)new_chunk+sizeof(struct memory_chunk_t)+FENCE_LENGTH;
    }
}
void  heap_free(void* memblock)
{
    if(memblock == NULL) {
        return;
    }
    struct memory_chunk_t * chunk = memory_manager.first_memory_chunk;
    while(chunk!=NULL&&(uint8_t*)(chunk + 1)+FENCE_LENGTH != (uint8_t *)memblock) {
        chunk = chunk->next;
    }
    if(chunk == NULL) {
        return;
    }
    if(chunk->next==NULL) {
        if(chunk->prev!=NULL) {
            chunk->prev->next=NULL;
            chunk->prev->checksum= calculate_checksum(chunk->prev);
        }
        else {
            memory_manager.first_memory_chunk = NULL;
        }
        chunk->free=1;
        chunk->checksum= calculate_checksum(chunk);
        struct memory_chunk_t * tail = chunk->prev;
        //int mem_size=memory_manager.memory_size;
        int size_to_free= ((uint8_t *)memory_manager.memory_start+ memory_manager.memory_size)-(uint8_t *)(chunk);
        memory_manager.memory_size-=size_to_free;
        custom_sbrk(-size_to_free);
        while(tail!=NULL&&tail->free==1) {
            struct memory_chunk_t * prev= tail->prev;
            if(prev==NULL) {
                memory_manager.first_memory_chunk = NULL;
            }
            else {
                prev->next=NULL;
                prev->checksum= calculate_checksum(prev);
            }
            //int mem_size=memory_manager.memory_size;
            //memory_manager.memory_size-=((uint8_t *)memory_manager.memory_start+memory_manager.memory_size-(uint8_t *)tail);
            //custom_sbrk(-((uint8_t *)memory_manager.memory_start+mem_size-(uint8_t *)tail));
            size_to_free= ((uint8_t *)memory_manager.memory_start+ memory_manager.memory_size)-(uint8_t *)(tail);
            memory_manager.memory_size-=size_to_free;
            custom_sbrk(-size_to_free);
            tail=prev;
        }
        return;
    }
    chunk->size = (size_t)((uint8_t*)(chunk->next) - (uint8_t*)(chunk + 1));
    chunk->free = 1;
    chunk->checksum= calculate_checksum(chunk);
    if(chunk->next!=NULL && chunk->next->free==1) {
        uint8_t *end;
        if(chunk->next->next==NULL) {
            end = ((uint8_t*)(memory_manager.memory_start) + memory_manager.memory_size);
        }
        else {
            end = (uint8_t*)(chunk->next->next);
        }
        struct memory_chunk_t * chunk_new_next = chunk->next->next;
        chunk->next = chunk_new_next;
        chunk->size = end-(uint8_t *)(chunk+1);
        chunk->checksum= calculate_checksum(chunk);
        if(chunk_new_next!=NULL) {
            chunk_new_next->prev = chunk;
            chunk_new_next->checksum= calculate_checksum(chunk_new_next);
        }
    }
    if(chunk->prev!=NULL && chunk->prev->free==1) {
        uint8_t *end;
        struct memory_chunk_t *prev = chunk->prev;
        struct memory_chunk_t *next = chunk->next;
        if(next==NULL) {
            end = ((uint8_t*)(memory_manager.memory_start) + memory_manager.memory_size);
        }
        else {
            end = (uint8_t*)next;
        }
        prev->size = (end - (uint8_t*)(prev + 1));
        prev->next=next;
        prev->checksum= calculate_checksum(prev);
        chunk=prev;
        chunk->checksum= calculate_checksum(chunk);
        if(next!=NULL) {
            next->prev = prev;
            next->checksum = calculate_checksum(next);
        }
    }
    struct memory_chunk_t * tail=chunk;
    while(tail->next!=NULL) {
        tail=tail->next;
    }
    while(tail!=NULL&&tail->free==1) {
        struct memory_chunk_t * prev= tail->prev;
        if(prev==NULL) {
            memory_manager.first_memory_chunk = NULL;
        }
        else {
            prev->next=NULL;
            prev->checksum= calculate_checksum(prev);
        }
        //memory_manager.memory_size-=((uint8_t *)memory_manager.memory_start+memory_manager.memory_size-(uint8_t *)tail);
        //custom_sbrk(-((uint8_t *)memory_manager.memory_start+memory_manager.memory_size-(uint8_t *)tail));
        int size_to_free= ((uint8_t *)memory_manager.memory_start+ memory_manager.memory_size)-(uint8_t *)(tail);
        memory_manager.memory_size-=size_to_free;
        custom_sbrk(-size_to_free);
        tail=prev;

    }
}
void heap_clean(void)
{
    if(memory_manager.memory_start==NULL)
    {
        return;
    }
    custom_sbrk(-memory_manager.memory_size);
    memory_manager.memory_start=NULL;
    memory_manager.memory_size=0;
    memory_manager.first_memory_chunk=NULL;
}
int heap_validate(void)
{
    if(memory_manager.memory_start==NULL)
    {
        return 2;
    }
    void * start = memory_manager.first_memory_chunk;
    if(start == NULL&&memory_manager.memory_size==0)
    {
        return 0;
    }
    else if(start==NULL&&memory_manager.memory_size!=0)
    {
        return 2;
    }
    void * current = start;
    while(1)
    {
        if((uint8_t *)current<(uint8_t *)memory_manager.memory_start||(uint8_t *)current> (uint8_t *)custom_sbrk(0))
        {
            return 3;
        }
        struct memory_chunk_t * current_chunk = (struct memory_chunk_t *)current;
        if(current_chunk->checksum != calculate_checksum(current_chunk))
        {
            return 3;
        }
        if(current_chunk->size>2147483647)
        {
            return 3;
        }
        if(current_chunk->free!=1&&current_chunk->free!=0)
        {
            return 3;
        }
        if(current_chunk->free==0)
        {
            uint8_t *fence_before= (uint8_t *)(current_chunk+1);
            if(fence_before<(uint8_t *)memory_manager.memory_start||fence_before> (uint8_t *)custom_sbrk(0))
            {
                return 3;
            }
            for(int i=0;i<16;i++) {
                //printf("%c ",*(fence_before+i));
                if(*(fence_before+i) != *(FENCE+i)) {
                    return 1;
                }
            }
            uint8_t *fence_after=((uint8_t *)(current_chunk+1))+current_chunk->size+FENCE_LENGTH;
            if(fence_after<(uint8_t *)memory_manager.memory_start||fence_after> (uint8_t *)custom_sbrk(0))
            {
                return 3;
            }
            for(int i=0;i<16;i++) {
                //printf("%c ",*(fence_before+i));
                if(*(fence_after+i) != *(FENCE+i)) {
                    return 1;
                }
            }
        }
        if(current_chunk->next==NULL)
        {
            return 0;
        }
        if((uint8_t *)current_chunk->next<(uint8_t *)memory_manager.memory_start||(uint8_t *)current_chunk->next>(uint8_t *)custom_sbrk(0))
        {
            return 3;
        }
        current = (void *)current_chunk->next;
    }
}
enum pointer_type_t get_pointer_type(const void* const pointer)
{
    if(pointer==NULL)
    {
        return pointer_null;
    }
    if(heap_validate()!=0)
    {
        return pointer_heap_corrupted;
    }
    struct memory_chunk_t *chunk = memory_manager.first_memory_chunk;
    while(chunk!=NULL)
    {
        uint8_t * header = (uint8_t *)chunk;
        for(int i=0;i<(int)sizeof(struct memory_chunk_t); i++)
        {
            if((header+i)==pointer)
            {
                return pointer_control_block;
            }
        }
        if(chunk->free==0)
        {
            uint8_t *fence_before= (uint8_t *)(chunk+1);
            for(int i=0;i<16;i++) {
                if((fence_before+i) == pointer) {
                    return pointer_inside_fences;
                }
            }
            uint8_t *fence_after=((uint8_t *)(chunk+1))+chunk->size+FENCE_LENGTH;

            for(int i=0;i<16;i++) {
                if((fence_after+i) == pointer) {
                    return pointer_inside_fences;
                }
            }
            uint8_t * data = (uint8_t *)(chunk+1)+FENCE_LENGTH;
            if(data==pointer)
            {
                return pointer_valid;
            }
            if((uint8_t *)pointer>data &&(uint8_t *)pointer<fence_after)
            {
                return pointer_inside_data_block;
            }
        }
        else
        {
            if((uint8_t *)pointer>(uint8_t *)(chunk+1)&&(uint8_t *)(chunk+1)+chunk->size>(uint8_t *)pointer)
            {
                return pointer_unallocated;
            }
        }
        chunk = chunk->next;

    }
    return pointer_unallocated;
}
size_t heap_get_largest_used_block_size(void)
{
    if(memory_manager.memory_start==NULL||memory_manager.memory_size==0||heap_validate()!=0||memory_manager.first_memory_chunk==0)
    {
        return 0;
    }
    struct memory_chunk_t *chunk = memory_manager.first_memory_chunk;
    size_t max_size=0;
    while(chunk!=NULL)
    {
        if(chunk->size>max_size&&chunk->free==0)
        {
            max_size = chunk->size;
        }
        chunk=chunk->next;
    }
    return max_size;
}
void* heap_calloc(size_t number, size_t size) {
    if(size == 0||number==0) {
        return NULL;
    }
    int size_chunk = number*size;
    if(memory_manager.first_memory_chunk == NULL)
    {
        void *request = custom_sbrk(size_chunk + sizeof(struct memory_chunk_t)+FENCE_LENGTH*2);
        memory_manager.memory_size+=size_chunk + sizeof(struct memory_chunk_t)+FENCE_LENGTH*2;
        if(request==(void *)-1)
        {
            return NULL;
        }
        struct memory_chunk_t * chunk=(struct memory_chunk_t *)request;
        chunk->next=NULL;
        chunk->prev=NULL;
        chunk->free=0;
        chunk->size=size_chunk;
        chunk->checksum= calculate_checksum(chunk);
        (memory_manager.first_memory_chunk)=chunk;
        uint8_t *fence_before= (uint8_t *)memory_manager.memory_start + sizeof(struct memory_chunk_t);
        for(int i=0;i<16;i++) {
            *(fence_before+i)=*(FENCE+i);
        }
        uint8_t *fence_after=fence_before+size_chunk+FENCE_LENGTH;
        for(int i=0;i<16;i++) {
            *(fence_after+i)=*(FENCE+i);
        }
        uint8_t *data = (uint8_t *)(chunk+1)+FENCE_LENGTH;
        for(int i=0;i<size_chunk;i++)
        {
            *(data+i)=0;
        }
        return (uint8_t *)memory_manager.memory_start+sizeof(struct memory_chunk_t)+FENCE_LENGTH;
    }
    else
    {
        struct memory_chunk_t * chunk=memory_manager.first_memory_chunk;
        while(1) {
            if(chunk->free==1) {
                if(chunk->size >= (size_t)size_chunk+FENCE_LENGTH*2) {
                    chunk->size=size_chunk;
                    chunk->free=0;
                    chunk->checksum= calculate_checksum(chunk);
                    uint8_t *fence_before= (uint8_t *)chunk + sizeof(struct memory_chunk_t);
                    for(int i=0;i<16;i++) {
                        *(fence_before+i)=*(FENCE+i);
                    }
                    uint8_t *fence_after=fence_before+size_chunk+FENCE_LENGTH;
                    for(int i=0;i<16;i++) {
                        *(fence_after+i)=*(FENCE+i);
                    }
                    uint8_t *data = (uint8_t *)(chunk+1)+FENCE_LENGTH;
                    for(int i=0;i<size_chunk;i++)
                    {
                        *(data+i)=0;
                    }
                    return (uint8_t *)chunk+sizeof(struct memory_chunk_t)+FENCE_LENGTH;
                }
            }
            if(chunk->next==NULL) {
                break;
            }
            chunk=chunk->next;
        }
        struct memory_chunk_t * new_chunk = custom_sbrk(size_chunk + sizeof(struct memory_chunk_t)+FENCE_LENGTH*2);
        if(new_chunk==(void *)-1)
        {
            return NULL;
        }
        memory_manager.memory_size+=size_chunk + sizeof(struct memory_chunk_t)+FENCE_LENGTH*2;
        new_chunk->free=0;
        new_chunk->size=size_chunk;
        new_chunk->next=NULL;
        new_chunk->prev=chunk;
        chunk->next = new_chunk;
        chunk->checksum= calculate_checksum(chunk);
        new_chunk->checksum= calculate_checksum(new_chunk);
        uint8_t *fence_before= (uint8_t *)new_chunk + sizeof(struct memory_chunk_t);
        for(int i=0;i<16;i++) {
            *(fence_before+i)=*(FENCE+i);
        }
        uint8_t *fence_after=fence_before+size_chunk+FENCE_LENGTH;
        for(int i=0;i<16;i++) {
            *(fence_after+i)=*(FENCE+i);

        }
        uint8_t *data = (uint8_t *)(new_chunk+1)+FENCE_LENGTH;
        for(int i=0;i<size_chunk;i++)
        {
            *(data+i)=0;
        }
        return (uint8_t *)new_chunk+sizeof(struct memory_chunk_t)+FENCE_LENGTH;
    }
}
void* heap_realloc(void* memblock, size_t size)
{
    if(heap_validate()!=0)
    {
        return NULL;
    }
    if(memblock==NULL)
    {
        return heap_malloc(size);
    }
    if(get_pointer_type(memblock)!=pointer_valid)
    {
        return NULL;
    }
    struct memory_chunk_t * chunk =(struct memory_chunk_t *)((uint8_t *)memblock-FENCE_LENGTH)-1;
    if(size==chunk->size)
    {
        return memblock;
    }
    if(size==0)
    {
        heap_free(memblock);
        return NULL;
    }
    if(size<chunk->size)
    {
        uint8_t * fence_after = (uint8_t *)(chunk+1)+FENCE_LENGTH+size;
        for(int i=0;i<16;i++) {
            *(fence_after+i)=*(FENCE+i);

        }
        if(chunk->next==NULL)
        {
            int shorten = chunk->size-size;
            custom_sbrk(-shorten);
            memory_manager.memory_size-=shorten;
        }
        chunk->size=size;
        chunk->checksum= calculate_checksum(chunk);
        return memblock;
    }
    if(chunk->next==NULL)
    {
        void *request = custom_sbrk(size-chunk->size);
        if(request==(void *)-1)
        {
            return NULL;
        }
        memory_manager.memory_size+=size-chunk->size;
        chunk->size =size;
        uint8_t * fence_after = (uint8_t *)(chunk+1)+FENCE_LENGTH+chunk->size;
        for(int i=0;i<16;i++) {
            *(fence_after+i)=*(FENCE+i);
        }
        chunk->checksum = calculate_checksum(chunk);
        return memblock;
    }
    int padding =((uint8_t *)(chunk->next)) -((uint8_t *)(chunk+1)+FENCE_LENGTH*2+chunk->size);
    if(chunk->size+padding>=size)
    {
        chunk->size=size;
        uint8_t * fence_after = (uint8_t *)(chunk+1)+FENCE_LENGTH+chunk->size;
        for(int i=0;i<16;i++) {
            *(fence_after+i)=*(FENCE+i);
        }
        chunk->checksum= calculate_checksum(chunk);
        return memblock;
    }
    if(chunk->next!=NULL&&chunk->next->free!=0&&(chunk->next->size+padding+chunk->size>=size))
    {
        struct memory_chunk_t * chunk_next = chunk->next;
        int needed_memory = size - chunk->size-padding;
        int total_new_B = sizeof(struct memory_chunk_t) + chunk_next->size-needed_memory;
        /*
        if(total_new_B<(int)sizeof(struct memory_chunk_t) +1)
        {
            chunk->next = chunk_next->next;
            chunk->size = size;
            uint8_t * fence_after = (uint8_t *)(chunk+1)+FENCE_LENGTH+chunk->size;
            for(int i=0;i<16;i++) {
                *(fence_after+i)=*(FENCE+i);
            }
            chunk->checksum=calculate_checksum(chunk);
            if(chunk->next!=NULL)
            {
                chunk->next->prev=chunk;
                chunk->next->checksum= calculate_checksum(chunk->next);
            }
            else
            {
                custom_sbrk(-total_new_B);
                memory_manager.memory_size-=total_new_B;
            }
            return memblock;
        }
        else
        {
         */
            struct memory_chunk_t * B_next = chunk_next->next;
            chunk->size = size;
            uint8_t * fence_after = (uint8_t *)(chunk+1)+FENCE_LENGTH+chunk->size;
            for(int i=0;i<16;i++) {
                *(fence_after+i)=*(FENCE+i);
            }
            struct memory_chunk_t * B = (struct memory_chunk_t *)(fence_after+FENCE_LENGTH);
            chunk->next=B;
            chunk->checksum = calculate_checksum(chunk);
            B->free=1;
            B->prev = chunk;
            B->next=B_next;
            B->size=total_new_B-sizeof(struct memory_chunk_t);
            if(B_next!=NULL)
            {
                B_next->prev=B;
                B_next->checksum= calculate_checksum(B_next);
            }
            B->checksum=calculate_checksum(B);
            return memblock;
        //}
    }
    void *new_memory= heap_malloc(size);
    if(new_memory==NULL)
    {
        return NULL;
    }
    for(int i=0;i<(int)chunk->size;i++)
    {
        *((uint8_t *)new_memory+i)=(*((uint8_t *)chunk+FENCE_LENGTH + sizeof(struct memory_chunk_t) + i));
    }
    heap_free((uint8_t *)(chunk+1)+FENCE_LENGTH);
    return new_memory;
    return NULL;
}
unsigned long long int calculate_checksum(const struct memory_chunk_t * const chunk)
{
    int suma=0;
    for(int i=0;(unsigned  long)i<sizeof(struct memory_chunk_t)-sizeof(unsigned long long int );i++)
    {
        suma+=*(((uint8_t *)chunk)+i);
    }
    return suma;
}



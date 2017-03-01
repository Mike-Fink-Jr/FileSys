////////////////////////////////////////////////////////////////////////////////
//
//  File           : cart_cache.c
//  Description    : This is the implementation of the cache for the CART
//                   driver.
//
//  Author         : Michael Fink Jr.
//  Last Modified  : 11/18/2016
//

// Includes
#include <stdlib.h>
#include <string.h>

// Project Includes
#include <cart_driver.h>
#include <cart_cache.h>
#include <cart_controller.h>
#include <cmpsc311_log.h> 

// Defines

//
// Functions

 typedef struct cache_node
{
	uint32_t file_num;// file num of the frame

	struct cache_node *prev, *next;//pointers to the prev and next nodes

	char frame[1024];//actual data in the cache

}cache_node; // node in a double linked list for LRU Cache

typedef struct Cache
 {  int8_t flag; //1 if power is on  or 0 if power is off

	int32_t max; // max number of frames in the cache
	
	int32_t cap; //capacity in the structure... should never be > max 
	
	cache_node *start, *end;

 }Cache;

 // initialize the cache globally since i cant pass the pointer around
Cache* cache=NULL;



cache_node* newNode(uint32_t file_num, void* buf)
{
	cache_node* node = (cache_node*)calloc(1,sizeof(cache_node));
	node->file_num=file_num;
	if(buf!=NULL)
		memcpy(node->frame,buf,1024);
	 
	 node->prev=node->next=NULL; // set to null initially, to be changed after called
	return node;
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : set_cart_cache_size
// Description  : Set the size of the cache (must be called before init)
//
// Inputs       : max_frames - the maximum number of items your cache can hold
// Outputs      : 0 if successful, -1 if failure

int set_cart_cache_size(uint32_t max_frames)
{
	if(max_frames<0)
		return(-1);
	if(max_frames==0)
		return(0);

	cache = (Cache*)(malloc(sizeof(Cache)));
	cache->max=max_frames;
	cache->flag=0;
	return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : init_cart_cache
// Description  : Initialize the cache and note maximum frames
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int init_cart_cache(void)
{
	if(cache==NULL)
	{	
	
		set_cart_cache_size(DEFAULT_CART_FRAME_CACHE_SIZE);	//sets to default if not -c flag in cmd line call
			

			if(cache==NULL)
				return(0);
	}

    if( cache->flag==0)
	 	cache->flag=1; 
    else
    {
    	logMessage(LOG_ERROR_LEVEL,"Error in init cache flag is already on");
        return(-1);
    }

    cache->start=cache->end=NULL; //no values, creates start pointing to end and both null


    cache->cap=-1;

	return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : close_cart_cache
// Description  : Clear all of the contents of the cache, cleanup
//
// Inputs       : none
// Outputs      : o if successful, -1 if failure

int close_cart_cache(void)
{
	if(cache==NULL)
		return(0);

	cache_node* next= cache->start;
	cache_node* curr;
	int i;

    if( cache->flag==1)
	 	cache->flag=0; 
    else
    {
    	logMessage(LOG_ERROR_LEVEL,"Error in close cache flag is already off");
        return(-1);
    }
	
	for(i=1;i<cache->max && i<cache->cap;i++)
	{	curr=next; //sets pointer curr too the next node in the series
		if(next!=cache->end)
			next=curr->next; // saves the next node
		else
			i=cache->max;// ensures exit of the for loop
				
		curr->next=curr->prev=NULL;//sets the pointers to null
		
		free(curr); // frees the allocated memory
		
	}//alll allocated nodes are deallocated

	cache->start=cache->end=NULL;
    free(cache);
    cache=NULL;

	return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : put_cart_cache
// Description  : Put an object into the frame cache
//
// Inputs       : file_num - cart*1024 + frm : flag for each specific frame
//                buf - the buffer to insert into the cache
// Outputs      : 0 if successful, -1 if failure

int put_cart_cache(uint32_t file_num, void *buf)  
{	
	if(cache==NULL)
		return(0);

	cache_node* node;
	int i;
	int x;
	if(cache->cap==-1)//cache is empty
	{	
		node= newNode(file_num,buf);
		node->prev = NULL;
		node->next = NULL;
		cache->start = node;
		cache->end = node;
		cache->cap=1;
		return(0);
	}
	node=cache->start; //needed for the next 2 scenarios
	
	if(cache->cap<cache->max)
		x=cache->cap;
	else
		x=cache->max;

		for(i=0;i<x;i++)
		{	if(node -> file_num == file_num)// check if already in the cache
			{	

				memcpy(node->frame,buf,1024);//copy the buffer to the node frame

				if(node==cache->start)// if at start dont do anything
					return(0);

				if(node==cache->end)// if at end update the new end
				{
					node -> prev -> next = NULL;
					cache->end=node->prev;
				}
				else	//node in the middle...update the prev to next and vise versa
				{
					node->next->prev= node->prev;
					node->prev->next=node->next;
				}

				//set node to the start of the list
				node->prev=NULL; // curr node==start == no previous node
				
				node->next=cache->start; // curr node next is set to the start
				
				cache->start->prev= node;// update start->prev to node
				
				cache->start=node;//start is set to curr node with prev=null and next = old start
				
				return(0); //end if same file_num
			}

			if(node->next!=NULL)//should never happen but just in case
				
				node=node->next;//go to next node
			
			else
			
				i=x; //force exit loop.... should never happen


		}//end for loop


		if(x==cache->cap)// if reaches this point create a new node
		{
			node= newNode(file_num,buf);
			
			node->next = cache->start;
			
			node->prev=NULL;

			cache->start->prev=node;

			cache->start = node;

			cache->cap= (cache->cap) + 1;
			
			return(0);
		}

	//past here we have a full cache and need to replace		

	//set the new cache end
	node=cache->end;
	
	cache->end=node->prev;
	
	cache->end->next=NULL;


	//overwrite the curr node with the new information
	node->file_num=file_num;//over right the file num
	
	memcpy(node->frame,buf,1024);//over right the buffer
	
	node -> prev = NULL;
	
	node -> next = cache -> start;

	//set the start info
	cache -> start -> prev = node;
	
	cache -> start=node;

	return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : get_cart_cache
// Description  : Get an frame from the cache (and return it)
//
// Inputs       : cart - the cartridge number of the cartridge to find
//                frm - the  number of the frame to find
// Outputs      : pointer to cached frame or NULL if not found

void * get_cart_cache( uint32_t file_num)
{	
	if(cache==NULL)
		return(0);

	if(cache->cap == -1)
		return NULL;
	cache_node* node=cache->start;
	int i;
	int x;

	if(cache->cap<cache->max)
		x=cache->cap;
	else
		x=cache->max;

	for(i=0;i<x;i++)
	{	if(file_num==node->file_num)
		{	if(node==cache->start)
				return(node->frame);
			if(node==cache->end)
			{
				node->prev->next= NULL;
				cache->end= node->prev;
			}
			else
			{
				node -> prev -> next = node -> next;
				node -> next -> prev = node ->prev;
			}

			node->next= cache->start;
			node->prev= NULL;
			cache->start->prev=node;
			cache->start=node;

			return(node->frame);
		}

		node=node->next;
	}

	//not in the cache
	return(NULL);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : delete_cart_cache
// Description  : Remove a frame from the cache (and return it)
//
// Inputs       : cart - the cart number of the frame to remove from cache
//                blk - the frame number of the frame to remove from cache
// Outputs      : pointe buffer inserted into the object

void * delete_cart_cache(CartridgeIndex cart, CartFrameIndex blk) {
return(NULL);
}

//
// Unit test

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cartCacheUnitTest
// Description  : Run a UNIT test checking the cache implementation
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int cartCacheUnitTest(void) {

	// Return successfully
	logMessage(LOG_OUTPUT_LEVEL, "Cache unit test completed successfully.");
	return(0);
}

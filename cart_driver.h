#ifndef CART_DRIVER_INCLUDED
#define CART_DRIVER_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File           : cart_driver.h
//  Description    : This is the header file for the standardized IO functions
//                   for used to access the CART storage system.
//
//  Author         : Patrick McDaniel
//  Last Modified  : Thu Sep 15 15:05:53 EDT 2016
//

// Include files
#include <stdint.h>
#include <cart_controller.h>

// Defines
#define CART_MAX_TOTAL_FILES 1024 // Maximum number of files ever
#define CART_MAX_PATH_LENGTH 128 // Maximum length of filename length

//
// Interface functions

int32_t cart_poweron(void);
	// Startup up the CART interface, initialize filesystem

int32_t cart_poweroff(void);
	// Shut down the CART interface, close all files

int16_t cart_open(char *path);
	// This function opens the file and returns a file handle

int16_t cart_close(int16_t fd);
	// This function closes the file

int32_t cart_read(int16_t fd, void *buf, int32_t count);
	// Reads "count" bytes from the file handle "fh" into the buffer  "buf"

int32_t cart_write(int16_t fd, void *buf, int32_t count);
	// Writes "count" bytes to the file handle "fh" from the buffer  "buf"

int32_t cart_seek(int16_t fd, uint32_t loc);
	// Seek to specific point in the file

//helper functions for cart communication
int16_t CNF(uint16_t n);
int16_t FNF(uint16_t n);
uint64_t  BS(uint16_t n, int s); 
CartXferRegister stitch(uint8_t ky1,uint8_t ky2,uint8_t rt1,uint16_t ct1,uint16_t fm1);
int unstitch(CartXferRegister reg, uint8_t *ky1, uint8_t *ky2,uint8_t *rt1, uint16_t *ct1, uint16_t *fm1);


//Universal helper Function to Load the Cart and check if loaded
uint16_t loadCart(uint16_t cartNum);


int initCache();
int initCart();
int zeroCart();
int powerOff();
int16_t findFile(char* path);
int32_t writer(uint16_t cart, uint16_t frame, void* buf);
int32_t reader(uint16_t cart, uint16_t frame, void* buf);



#endif



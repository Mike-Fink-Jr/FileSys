////////////////////////////////////////////////////////////////////////////////
//
//  File           : cart_driver.c
//  Description    : This is the implementation of the standardized IO functions
//                   for used to access the CRUD storage system.
//
//  Author         : Michael John Fink Jr.
//  Last Modified  : 12/7/2016
//

// Includes
#include <stdlib.h>
#include <string.h>

// Project Includes
#include <cart_cache.h>
#include <cart_driver.h>
#include <cart_controller.h> 
#include <cart_network.h>
#include <cmpsc311_log.h>
//Structure 


// files construct
struct Filer{
     char path[128];
     uint16_t start;//start num
     uint16_t file_num;    
     uint16_t file_pos;
     uint32_t length;// total length of the file
     int8_t used;
     uint32_t curr_len;
     int8_t offset;

    
} myFiles[ CART_MAX_TOTAL_FILES ];//file handle is the positon in the myFiles array   


// data storage table
struct Table
{   uint16_t new;
    int8_t cache_flag;
    int8_t flag; //1 if power is on  or 0 if power is off
    int16_t cUsed[CART_MAX_CARTRIDGES];//number of frames used full in carts
    CartridgeIndex cI;//current cart index
    struct Cartridge
     {  
         uint16_t fUsed[CART_CARTRIDGE_SIZE];//# data written in each frame       
         uint16_t next[CART_CARTRIDGE_SIZE]; 
        
     } cart[CART_MAX_CARTRIDGES];
}tab;
  

    // global declarations
    CartXferRegister sReg=0;//Register sent to the bus needs to be stitched
    CartXferRegister rReg=0;//Register returned from the bus need to be unstitched
    uint8_t rKY1=0;
    uint8_t rKY2=0;
    uint8_t rRT1=0;
    uint16_t rCT1=0;
    uint16_t rFM1=0;
    

//
// Implementation

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//Function Declaration
//////////////////////////////////////////////////////////////////////////////////////////////////////////
    
//helper functions for cart communication
int16_t CNF(uint16_t n);
int16_t FNF(uint16_t n);
uint64_t  BS(uint16_t n, int s); 
CartXferRegister stitch(uint8_t ky1,uint8_t ky2,uint8_t rt1,uint16_t ct1,uint16_t fm1);
int unstitch(CartXferRegister reg, uint8_t *ky1, uint8_t *ky2,uint8_t *rt1, uint16_t *ct1, uint16_t *fm1);


//Universal helper Function to Load the Cart and check if loaded
uint16_t loadCart(uint16_t cartNum);

// cart_power_on/off and helper functions
int32_t cart_poweron(void);
int32_t cart_poweroff(void); 
int initCache();
int initCart();
int zeroCart();
int powerOff();


//cart_open/close and helper functions
int16_t cart_open(char* path);
int16_t cart_close(int16_t fd);
int16_t findFile(char* path);


//cart_read/write and helper functions
int32_t cart_read(int16_t fd, void *buf, int32_t count);
int32_t cart_write(int16_t fd, void *buf, int32_t count);
int32_t cart_seek(int16_t fd, uint32_t loc); 
int32_t writer(uint16_t cart, uint16_t frame, void* buf);
int32_t reader(uint16_t cart, uint16_t frame, void* buf);




//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////


int initCache()
{   
    
    
    int x=init_cart_cache();
    if(x == -1)    
    {   
      close_cart_cache();
      logMessage(LOG_ERROR_LEVEL,"Error @ cache init ");
      return(-1);
    }
    return(0);
}



////////////////////////////////////////////////////////////////////////////////
//
// Function     : CNF   (Cart Next Finder)
// Description  :  given file num n return the cart= n / 1024  
//
// Inputs       : uint16_t       n (file_num)
//
// Outputs      : int16_t        cart Position
//
int16_t CNF(uint16_t n)  { return n / 1024;}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : FNF   (Frame Next Finder)
// Description  :  given file num n return the frame= n mod 1024  
//
// Inputs       : uint16_t       n (file_num)
//
// Outputs      : int16_t        frame in cart
//
int16_t FNF(uint16_t n)  {return n % 1024;}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : BS   (bit shifter)
// Description  : Helper function for stitch and unstich, shift bits s amount
//
// Inputs       : uint16_t       n (number to shift)  
//                int            s (shift amount)
//
// Outputs      : packed CartXferRegister
//
uint64_t  BS(uint16_t n, int s)  {return (uint64_t)n<<s;}
 

////////////////////////////////////////////////////////////////////////////////
//
// Function     : stitchOP
// Description  : creates a OP code to send to the IO Bus
//
// Inputs       : uint8_t        ky1,ky2,rt1 
//                uint16_t       ct1,fm1
//
// Outputs      : packed CartXferRegister
//
CartXferRegister stitch(uint8_t ky1,uint8_t ky2,uint8_t rt1,uint16_t ct1,uint16_t fm1)
{
    CartXferRegister code= BS(ky1,56);
    code =code | BS(ky2,48) | BS(rt1,47) | BS(ct1,31) | BS(fm1,15);
    return code;
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : unstitchOP
// Description  : takes apart a opcode recieved from the bus & sends to their pointers
//
// Inputs       : CartXferRegister   reg
//                uint8_t*           ky1,ky2,rt1 
//                uint16_t*          ct1,fm1
// Outputs      : 1 if successful, -1 if failure
//
int unstitch(CartXferRegister reg, uint8_t *ky1, uint8_t *ky2,uint8_t *rt1, uint16_t *ct1, uint16_t *fm1)
{   
    uint64_t temp=0;
    
    *ky1= ((uint8_t) (reg >> 56));
        temp= reg << 8;
    *ky2 = ((uint8_t)(temp >> 56));
        temp = reg << 16;
    *rt1 =((uint8_t) (temp >> 63));
        temp = reg << 17;
    *ct1 = ((uint16_t)(temp >> 48));
        temp = reg << 33;
    *fm1 = ((uint8_t)(temp >> 48));

    return(0);    
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : initCart
// Description  : performs the CART_OP_INITMS op code and outputs
//
// Inputs       : none
// Outputs      : 1 if successful, -1 if failure
//
int initCart()
{
 //check if already turned on
    if(tab.flag==1)
    { 

         logMessage(LOG_ERROR_LEVEL,"Error in cart_poweron power is already on");
         return(-1);
    }

    sReg= stitch(CART_OP_INITMS,0,0,0,0);
   // rReg=cart_io_bus(sReg,NULL);
    rReg=cart_client_bus_request(sReg, NULL);

    if( unstitch(rReg,&rKY1,&rKY2,&rRT1,&rCT1,&rFM1))
    { 
         logMessage(LOG_ERROR_LEVEL,"Error in cart_poweron power is already on");
         return(-1);
    }
    if(rRT1!=0)
    { 
         logMessage(LOG_ERROR_LEVEL,"Error rRT1 = %u is not 0 after initms", rRT1);
         return(-1);
    }
    tab.flag=1;
    return(0);
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : zeroCart
// Description  : performs the CART_OP_BZERO op code and outputs
//
// Inputs       : none
// Outputs      : 1 if successful, -1 if failure
int zeroCart()
{  //switch cartridges

    sReg= stitch(CART_OP_BZERO,0,0,0,0);
  //  rReg= cart_io_bus(sReg,NULL);
    rReg=cart_client_bus_request(sReg, NULL);

    if( unstitch(rReg,&rKY1,&rKY2,&rRT1,&rCT1,&rFM1))
    { 
        logMessage(LOG_ERROR_LEVEL,"Error in zeroCart" );
        return(-1);
    }

    if(rRT1!=0)
    { 
        logMessage(LOG_ERROR_LEVEL,"Error rRT1 is not 0 @ zerocart ");
        return(-1);
    }
   
    tab.cUsed[tab.cI]=0;
    
    int x=0;
    
    //zero the Table tab where cart is zerod
    for(x=0;x<1024;x++)//access all frames in curr cart
    { 
        tab.cart[tab.cI].fUsed[x]=0;//all frames have 0 bits written
        tab.cart[tab.cI].next[x]=-1;//all next values are null
    }

    return(0);
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : loadCart
// Description  : performs the CART_OP_LDCART op code and outputs
//
// Inputs       : uint16_t cartNum
//
// Outputs      : 1 if successful, -1 if failure
//
uint16_t loadCart(uint16_t cartNum)
{  //switch cartridges
    if(tab.cI==cartNum) return(1);

    sReg= stitch(CART_OP_LDCART,0,0,cartNum,0);
   // rReg= cart_io_bus(sReg,NULL);
    rReg=cart_client_bus_request(sReg, NULL);
      
    if( unstitch(rReg,&rKY1,&rKY2,&rRT1,&rCT1,&rFM1))
    { 
        logMessage(LOG_ERROR_LEVEL,"Error in loadCart Cart " );
        return(-1);
    }

    if(rRT1!=0)
    { 
        logMessage(LOG_ERROR_LEVEL,"Error rRT1 is not 0 @ ld cart");
        return(-1);
    }
    tab.cI=cartNum;
    return(0);
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : powerOff
// Description  : performs the CART_OP_POWOFF op code and outputs success
//
// Inputs       : none
// Outputs      : 1 if successful, -1 if failure
int powerOff()
{   


      logMessage(LOG_INFO_LEVEL,"\n\n Cart Command- Power Off\n\n ");

    sReg= stitch(CART_OP_POWOFF,0,0,0,0);
   // rReg= cart_io_bus(sReg,NULL);
    rReg=cart_client_bus_request(sReg, NULL);

    if( unstitch(rReg,&rKY1,&rKY2,&rRT1,&rCT1,&rFM1))
    { 
        logMessage(LOG_ERROR_LEVEL,"Error in powerOff OP code is bad:" );
        return(-1);
    }

    if(rRT1!=0)
    { 
        logMessage(LOG_ERROR_LEVEL,"Error rRT1 is not 0 @ poweroff");
        return(-1);
    }
    tab.flag=0;
    return(1);

}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_poweron
// Description  : Startup up the CART interface, initialize filesystem
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure
int32_t cart_poweron(void) {
   
    if(initCart()!=0)
     {   
        logMessage(LOG_ERROR_LEVEL,"Error rRT1 is not 0 @ initCart");
        return(-1);
     }


    //cache initializer, set cache size in function at top of the file
    initCache();


    uint16_t c;
    tab.cI=-1;

    // zero the memory  use  CART_OP_BZERO
    for( c=0; c < CART_MAX_CARTRIDGES; c++)
    {
        if(loadCart(c)!=0)
        {   
            close_cart_cache();
            logMessage(LOG_ERROR_LEVEL,"Error rRT1 is not 0 @ ld Cart");
            return(-1);
        }

       if(zeroCart()!=0)
        {   
            close_cart_cache();
            logMessage(LOG_ERROR_LEVEL,"Error rRT1 is not 0 @ zero cart");
            return(-1);
        }
    }    

    //STEP 4:: set up the data structure
    tab.flag=1;// flag that the power is on
    // tab.cache_flag=0;
    tab.new=0;
    loadCart(0);   
    
    for(c=0;c<CART_MAX_TOTAL_FILES;c++)
    {        
        myFiles[c].used=-1;    
        myFiles[c].start=0;    
        myFiles[c].file_num=0;    
        myFiles[c].file_pos=0;    
        myFiles[c].length=0;     
        myFiles[c].offset=0;
    }
	// Return successfully
	return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_poweroff
// Description  : Shut down the CART interface, close all files
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure
int32_t cart_poweroff(void) 
{

    //STEP 1:: check if power on
    if(tab.flag==-1)//already on
    {
        close_cart_cache();
        logMessage(LOG_ERROR_LEVEL,"Error rRT1 is not 0 @ zero cart");
        return( -1);
    }
    
    //STEP 2:: Clean up Data structure
    if(tab.cache_flag==1)// cache on
        if(close_cart_cache()==-1)
        {   
            close_cart_cache(); 
            logMessage(LOG_ERROR_LEVEL,"Error @ cache close ");
            return(-1);
        }

    //STEP 3:: Close all files
    int i;
    for(i=0;i<CART_MAX_TOTAL_FILES;i++)
       myFiles[i].used=0;

    //STEP 4:: Power off memory system
    powerOff();
     
	// Return successfully
	return(0);
}
 

 
////////////////////////////////////////////////////////////////////////////////
//
// Function     : findFile
// Description  : Helper function for cart_open, finds the file handle from the path
//
// Inputs       : path - filename of the file to open
// Outputs      : file handle or -1 if no exist
int16_t findFile(char* path)
{ 
    int16_t x;
    for(x=0;x<CART_MAX_TOTAL_FILES;x++)
    { 
        if((myFiles[x].used > -1) && (strncmp(myFiles[x].path,path,128)==0))
            return x;
    } 
    return -1;
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_open
// Description  : This function opens the file and returns a file handle
//
// Inputs       : path - filename of the file to open
// Outputs      : file handle if successful, -1 if failure

int16_t cart_open(char *path) {
    
    int16_t f=findFile(path); //f= file handle

    //STEP 1:: search to see if file exists
     if(f>-1)
     {
          //STEP 2:: check if file is open 
        if(myFiles[f].used==1)//file already open
        {        
            close_cart_cache();
            logMessage(LOG_ERROR_LEVEL,"Error file already open @cart_open");
            return(-1);
        }
        else
        {
            //file is closed and exists f= the file#
            myFiles[f].used=1;
            myFiles[f].file_num = myFiles[f].start;
                   // myFiles[f].file_pos = myFiles[f].start_pos;
            return(f); //return file handle
        }
    }
    else 
    if(f==-1)//file does not exist so create
    {
        f++;//f now is 0
        while(myFiles[f].used!=-1)
            f++;// find first unused file struct
        
        strncpy(myFiles[f].path,path,128);//copy path to Filer.path
        myFiles[f].used=1;   // set used to open
        myFiles[f].start=tab.new; 
        tab.new++; //set the start loc to last write
        myFiles[f].file_num = myFiles[f].start; //set curr write num to the start
        myFiles[f].file_pos = 0;//set the curr write pos to the start pos
        myFiles[f].length = 0;// set the length of the file to 0
        return(f);//return file handle
    }

    return (-1);
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_close
// Description  : This function closes the file
//
// Inputs       : fd - the file descriptor
//
// Outputs      : 0 if successful, -1 if failure
//
int16_t cart_close(int16_t fd) {

    //STEP 1:: check if legit file handle
    if((fd>4||fd<0) ||myFiles[fd].used==-1)
    {       
        logMessage(LOG_ERROR_LEVEL,"Error @cart_close bad file handle");
    	return (-1);
    }    
    //STEP 2:: check if file is open
    if(myFiles[fd].used==0)//used==0 means it is closed
    {       
        logMessage(LOG_ERROR_LEVEL,"Error @cart_close file already closed");
      	return (-1);
    }
    //STEP 3:: set flag to close position
    if(myFiles[fd].used==1) // used==1 means it is closed
        myFiles[fd].used=0;
    
	// Return successfully
	return (0);
}


int32_t reader(uint16_t cart, uint16_t frame, void* buf)
{

    loadCart(cart);//check that cartridge is good and sets cI
 

     
    void* t_buf=NULL;  
        t_buf=get_cart_cache(cart*1024+frame);   

    if(t_buf!=NULL)
    {   
        memcpy(buf,t_buf,1024);
        return(0); //CACHE HIT!!!
    }

    if(tab.cart[tab.cI].fUsed[frame]==0)
        return (0);

    //else cache miss :(
    sReg= stitch(CART_OP_RDFRME,0,0,0,frame);
    //rReg= cart_io_bus(sReg,buf);
    rReg=cart_client_bus_request(sReg, buf);
     
    if( unstitch(rReg,&rKY1,&rKY2,&rRT1,&rCT1,&rFM1))
    {       
        close_cart_cache();
        logMessage(LOG_ERROR_LEVEL,"Error in readFrame in write frame"); 
        return(-1);
    }
    if(rRT1!=0)
    {       
        close_cart_cache();
        logMessage(LOG_ERROR_LEVEL,"Error( rRT1 != 0) @readframe in write frame");
        return(-1);
    }
  
    return(0);
}


int32_t writer(uint16_t cart, uint16_t frame, void* buf)
{    //write myBuf to the frame
    loadCart(cart);//check that cartridge is good and sets cI
    sReg= stitch(CART_OP_WRFRME,0,0,0,frame);
   // rReg= cart_io_bus(sReg,buf);
    rReg=cart_client_bus_request(sReg, buf);

    if( unstitch(rReg,&rKY1,&rKY2,&rRT1,&rCT1,&rFM1))
    {       
        close_cart_cache();
        logMessage(LOG_ERROR_LEVEL,"Error in write writeFrame "); 
        return(-1);
    }
    if(rRT1!=0)
    { 
        close_cart_cache();
        logMessage(LOG_ERROR_LEVEL,"Error( rRT1 != 0) @ writeframe");
        return(-1);
    }

        if(put_cart_cache(cart*1024+frame, buf)==-1)
        {    
            close_cart_cache();
            logMessage(LOG_ERROR_LEVEL,"Errror @ cache put");
            return(-1);
        }

    return(0);
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_read
// Description  : Reads "count" bytes from the file handle "fh" into the 
//                buffer "buf"
//
// Inputs       : fd - filename of the file to read from
//                buf - pointer to buffer to read into
//                count - number of bytes to read
// Outputs      : bytes read if successful, -1 if failure
//
int32_t cart_read(int16_t fd, void *buf, int32_t count) {

    //STEP 1:: check if file is open 
    if(myFiles[fd].used==0)
    {       
        close_cart_cache();
        logMessage(LOG_ERROR_LEVEL,"Error @cart_read bad file handle, file is closed");
        return (-1);
    }    
    
    //STEP 2:: Check if file handle is legit 
    if(myFiles[fd].used==-1)
    {   
        close_cart_cache();    
        logMessage(LOG_ERROR_LEVEL,"Error @cart_read bad file handle, file no exist");
        return (-1);
    }    
    
    //STEP 3:: find memory position           
    char tbuf[count];
    char tempBuf[1024];
    int32_t read=0;  
    int i=myFiles[fd].file_pos;     

    while(count+i>=1024)
    {   
      
        reader(CNF(myFiles[fd].file_num), FNF(myFiles[fd].file_num),tempBuf);
        memcpy(&tbuf[read],&tempBuf[i],1024-i);//copy the next frame to my buf
        myFiles[fd].file_num= tab.cart[tab.cI].next[FNF(myFiles[fd].file_num)];
        myFiles[fd].file_pos=0;
        read=  read+1024-i;
        count= count-1024+i;
        i=0;
    } 
    if( count+i < 1024 ) //second case      // u end in the middle count
    {   
        //myBuf is the starting value of the read
        reader(CNF(myFiles[fd].file_num) , FNF(myFiles[fd].file_num), tempBuf);
      
        memcpy(&tbuf[read], &tempBuf[i] ,count);
        myFiles[fd].file_pos= i+count;
        read+= count;  
    }//exit and return read

    memcpy(buf,tbuf,read);
	return (read);
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_write
// Description  : Writes "count" bytes to the file handle "fh" from the 
//                buffer  "buf"
//
// Inputs       : fd - filename of the file to write to
//                buf - pointer to buffer to write from
//                count - number of bytes to write
// Outputs      : bytes written if successful, -1 if failure
//
int32_t cart_write(int16_t fd, void *buf, int32_t count) 
{
    if(myFiles[fd].used==0)
    {
        close_cart_cache();
        logMessage(LOG_ERROR_LEVEL,"Error @cart_read bad file handle, file is closed");
        return (-1);
    }    
    if(myFiles[fd].used==-1)
    {
        close_cart_cache();
        logMessage(LOG_ERROR_LEVEL,"Error @cart_read bad file handle, file no exist");
        return (-1);
    }//correct file handle      
   
    char tbuf[count];
    memcpy(tbuf,buf,count);// move contents to a char array
    char myBuf[1024];
    int i=myFiles[fd].file_pos; // i =0 if new frame, or the file pos
    int written=0;

    reader(CNF(myFiles[fd].file_num), FNF(myFiles[fd].file_num),myBuf);

    // first iteration, read the fram
    while(count+i>=1024)//middle bulk
    {     
        if(tab.cart[CNF(myFiles[fd].file_num)].next[FNF(myFiles[fd].file_num)] == 65535 )
            myFiles[fd].offset=0;
        else
            myFiles[fd].offset=1;
       
        memcpy(&myBuf[i],&tbuf[written],1024-i);

        writer(CNF(myFiles[fd].file_num), FNF(myFiles[fd].file_num),myBuf);
        //frame written and full, need to set next and fused and cUsed and length
        
        if(myFiles[fd].offset==0)//no offset update filing info
        {
            tab.cart[tab.cI].fUsed[FNF(myFiles[fd].file_num)]= 1023;        
            tab.cart[tab.cI].next[FNF(myFiles[fd].file_num)]= tab.new;
            tab.new++;
            myFiles[fd].length+=1024-i;    
            tab.cUsed[tab.cI]++; 
            //myFiles[fd].offset=1; 
        } 
       
        myFiles[fd].file_num= tab.cart[tab.cI].next[FNF(myFiles[fd].file_num)];
        myFiles[fd].file_pos=0;
        myFiles[fd].curr_len+=1024-i;
        written+=(1024-i);    
        count-=(1024-i);
        i=0;//set i because all positions will be 0 until end
        
        //read in the next frame for the loop
        reader(CNF(myFiles[fd].file_num), FNF(myFiles[fd].file_num),myBuf);
    }

       
    if(count+i<1024)//last part
    {
        if(myFiles[fd].length<(myFiles[fd].curr_len+i+count))
            myFiles[fd].offset=0;
        
        memcpy(&myBuf[i],&tbuf[written],count);

        //write myBuf to the frame
        writer(CNF(myFiles[fd].file_num),FNF(myFiles[fd].file_num),myBuf);
        myFiles[fd].curr_len+=count;
        if(myFiles[fd].offset==0)
        {//no offset
            myFiles[fd].length+= myFiles[fd].curr_len;
            tab.cart[tab.cI].fUsed[FNF(myFiles[fd].file_num)]= count+i;     
        }  //else u r overwrighting and not using more memory
          
        written+= count;
        myFiles[fd].file_pos+=count;
    }

    return(written); 
}




////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_seek
// Description  : Seek to specific point in the file
//
// Inputs       : fd - filename of the file to write to
//                loc - offfset of file in relation to beginning of file
// Outputs      : 0 if successful, -1 if failure
int32_t cart_seek(int16_t fd, uint32_t loc) 
{
    int i;

    if(myFiles[fd].used==0)
    {
        close_cart_cache();
        logMessage(LOG_ERROR_LEVEL,"Error @cart_seek bad file handle, file is closed");
        return (-1);
    }    

    if(myFiles[fd].used==-1)
    {
        close_cart_cache();
        logMessage(LOG_ERROR_LEVEL,"Error @cart_seek bad file handle, file no exist");
        return (-1);
    }//correct file handle  
 
    //STEP 3:: check if position is in the file 
    if(loc> myFiles[fd].length)
    {
        close_cart_cache();
        logMessage(LOG_ERROR_LEVEL,"Error @cart_seek loc>length");
        return (-1);
    }       


   /* myFiles[fd].offset=1;//set the offset flag to on(1)
    //STEP 4:: reset position
    myFiles[fd].file_num= loc/1024;
    myFiles[fd].file_pos= loc%1024;
    */
    if(loc==myFiles[fd].length)
        myFiles[fd].offset=0;
    else
        myFiles[fd].offset=1;  

    myFiles[fd].file_num= myFiles[fd].start;
    
    for(i=0; i<(loc/1024);i++)
        myFiles[fd].file_num = tab.cart[CNF(myFiles[fd].file_num)].next[FNF(myFiles[fd].file_num)]; 
    
    myFiles[fd].curr_len=loc;
    
    myFiles[fd].file_pos = loc % 1024; 
    // Return successfully
	  return (0);
}

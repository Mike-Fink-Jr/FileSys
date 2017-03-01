////////////////////////////////////////////////////////////////////////////////
//
//  File          : cart_client.c
//  Description   : This is the client side of the CART communication protocol.
//
//   Author       : Michael John Fink Jr.
//  Last Modified : 12/7/2016
//

// Include Files
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>

// Project Include Files
#include <cart_network.h>
#include <cmpsc311_util.h>
#include <cmpsc311_log.h> 
#include <cart_driver.h>
//
//  Global data
int   client_socket = -1;
struct sockaddr_in client_addr;
char* ip = CART_DEFAULT_IP;

int  cart_network_shutdown = 0;   // Flag indicating shutdown
unsigned char     *cart_network_address = NULL; // Address of CART server
unsigned short     cart_network_port = 0;       // Port of CART serve
unsigned long      CartControllerLLevel = LOG_INFO_LEVEL; // Controller log level (global)
unsigned long      CartDriverLLevel = 0;     // Driver log level (global)
unsigned long      CartSimulatorLLevel = 0;  // Driver log level (global)

//
// Functions

int32_t socket_ops()
{
	//there is no open connection.
	//(a) Setup the address
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(CART_DEFAULT_PORT);


	if( inet_aton(ip, &client_addr.sin_addr) == 0 )
	{
		logMessage(LOG_ERROR_LEVEL, "error in socket_ops address creation" );
		return (-1);
	} 

	//(b) Create the socket
	client_socket = socket(PF_INET,SOCK_STREAM,0); 
	if(client_socket == -1)
	{	
		logMessage(LOG_ERROR_LEVEL, "error in socket_ops on socket creation" );
		return (-1);
	} 

	//(c) Create the connection
    if(connect(client_socket, (const struct sockaddr *) &client_addr, sizeof(client_addr)) == -1)
	{
		logMessage(LOG_ERROR_LEVEL, "error in socket_ops connection failed" );
		return (-1);
	} 

	return (0);
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : client_cart_bus_request
// Description  : This the client operation that sends a request to the CART
//                server process.   It will:
//
//                1) if INIT make a connection to the server
//                2) send any request to the server, returning results
//                3) if CLOSE, will close the connection
//
// Inputs       : reg - the request reqisters for the command
//                buf - the block to be read/written from (READ/WRITE)
// Outputs      : the response structure encoded as needed

CartXferRegister cart_client_bus_request(CartXferRegister reg, void *buf)
{
	 if( client_socket == -1)
  		if(socket_ops() ==-1)
  		{
  			logMessage(LOG_ERROR_LEVEL, "error in socket_ops" );	
			return (-1);
  		}


     uint8_t   ky1, ky2, rt1;
     uint16_t ct1,fm1;
     
     unstitch(reg, &ky1,&ky2,&rt1,&ct1,&fm1);

  	 CartXferRegister net_reg =htonll64(reg);

	 switch(ky1)
	 {

 		case CART_OP_RDFRME: // CASE 1: RD operation
	 		// Send the register reg to the network after converting the register to 'network format'.
	    	// SEND: (reg) <- Network format 
 			if(write(client_socket, &net_reg, sizeof(net_reg)) != sizeof(net_reg))
 			{
				logMessage(LOG_ERROR_LEVEL, "error in RDFRME write1 failed");
				return(-1);
	
 			}

	    	// RECEIVE: (reg) -> Host format
 			if(read(client_socket, &net_reg, sizeof(net_reg)) != sizeof(net_reg))
 			{
				logMessage(LOG_ERROR_LEVEL, "error in RDFRME read1 failed");
				return(-1);
	
 			}
    	    // RECEIVE: 1024 frame (Data read from that frame)
 			if(read(client_socket, buf, 1024) != 1024)
 			{
				logMessage(LOG_ERROR_LEVEL, "error in RDFRME read2 failed");
				return(-1);
	
 			}

		break;

 		case CART_OP_WRFRME: // CASE 2: WR operation
			// Send the register reg to the network after converting the register to 'network format'.
			// SEND: (reg) <- Network format
 		 	if(write(client_socket, &net_reg, sizeof(net_reg)) != sizeof(net_reg))
 			{
				logMessage(LOG_ERROR_LEVEL, "error in WRFRME write1 failed");
				return(-1);
	
 			}

	    	//SEND: buf 1024 (Data to write to that frame)
 			if(write(client_socket, buf, 1024) != 1024)
 			{
				logMessage(LOG_ERROR_LEVEL, "error in WRFRME write2 failed");
				return(-1);
	
 			}

	    	// RECEIVE:
	    	// (reg) -> Host format
 			if(read(client_socket, &net_reg, sizeof(net_reg)) != sizeof(net_reg))
 			{
				logMessage(LOG_ERROR_LEVEL, "error in WRFRME read1 failed");
				return(-1);
	
 			}



		break; 

 		case CART_OP_POWOFF: // CASE 3: SHUTDOWN operation
			// Send the register reg to the network after converting the register to 'network format'.
			// SEND: (reg) <- Network format 
 			if(write(client_socket, &net_reg, sizeof(net_reg)) != sizeof(net_reg))
 			{
				logMessage(LOG_ERROR_LEVEL, "error in RDFRME write1 failed");
				return(-1);
	
 			}


	    	// RECEIVE:
	    	// (reg) -> Host format
 			if(read(client_socket, &net_reg, sizeof(net_reg)) != sizeof(net_reg))
 			{
				logMessage(LOG_ERROR_LEVEL, "error in RDFRME read1 failed");
				return(-1);
			}


			// reset socket_handle to initial value of -1.
			// Close the socket when finished (SHUTDOWN) : 
		     close(client_socket);
			 client_socket =-1;

		break;

 		default: // CASE 4: Other operation
			// Send the register reg to the network after converting the register to 'network format'.
			// SEND: (reg) <- Network format 
			if(write(client_socket, &net_reg, sizeof(net_reg)) != sizeof(net_reg))
	 			{
					logMessage(LOG_ERROR_LEVEL, "error in RDFRME write1 failed");
					return(-1);
		
	 			}

		    	// RECEIVE:
		    	// (reg) -> Host format
			if(read(client_socket, &net_reg, sizeof(net_reg)) != sizeof(net_reg))
			{
				logMessage(LOG_ERROR_LEVEL, "error in RDFRME read1 failed");
				return(-1);
			}

			unstitch(ntohll64(net_reg), &ky1, &ky2, &rt1, &ct1, &fm1);
		
	 }

	return ntohll64(net_reg);

}
/*********************  P r o g r a m  -  M o d u l e ***********************/
/*!
 *        \file  men_uart_ctrl.c
 *
 *      \author  andreas.werner@men.de
 *        $Date: 2013/12/18
 *
 *        \brief Tool to switch between the UART modes for legacy serial I/O
 *                 interfaces in MEN box PCs, FPGA-controlled.
 *
 *     Switches: -
 *
 *---------------------------------------------------------------------------
 * Copyright (c) 2013-2019, MEN Mikro Elektronik GmbH
 ****************************************************************************/
/*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/serial.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/io.h>

/*----------------+
| D E F I N E S   |
+----------------*/
#define VERSION 1

#define Z25_ACR         0x07	/* Additional Control Register offset */
#define Z25_MODE_SE     0x01	/* single ended (RS232)               */
#define Z25_MODE_FDX    0x05	/* differential, full duplex          */
#define Z25_MODE_HDX    0x0f	/* differential, half duplex, no echo */ 

#define SE_MODE         1
#define DIFF_HDX_MODE   2
#define DIFF_FDX_MODE   3

/*---------------+
| G L O B A L S  |
+---------------*/
/* MEN FPGA Legacy UART Ports */
unsigned int uart_ports[] = { 0x3f8,0x2f8,0x3e8,0x2e8,0x220 };


/**********************************************************************/
/** Usage Function. Print the usage of the tool
 *
 */
void usage()
{
	printf("========================================================\n");
	printf("men_uart_ctrl: switch between UART modes                \n");
	printf("                                                        \n");
	printf("- for legacy UART interfaces in MEN Box PCs, FPGA-controlled\n");
	printf("                                                        \n");
	printf("(c) 2013 by MEN Mikro Elektronik GmbH, Version: %d      \n", VERSION);
	printf("========================================================\n");
	printf("                                                        \n");
	printf("Usage: men_uart_ctrl -d <Device> [-m <mode>] [-r] [-x]  \n");
	printf("      -d <device> : tty device, e.g., /dev/ttyS0        \n");
	printf("      -m <mode>   : Set one of the following modes      \n");
	printf("            <1> - RS232                                 \n");
	printf("            <2> - RS422/RS485 half duplex               \n");
	printf("            <3> - RS422/RS485 full duplex               \n");
	printf("                                                        \n");
	printf("      -r  : Read current settings from device           \n");
	printf("      -x  : Read in raw format (return mode number only)\n");
	printf("========================================================\n");
	printf("                                                        \n");
}

/**********************************************************************/
/** UART Port Validation Function.
 *  This function validates the ports available on the product's FPGA.
 *
 *  \param port    Address of the UART
 *
 *  \return 1=Valid UART port, 0=No valid UART port
 */
int validatePort(unsigned int port)
{
	int i;

	for (i = 0; i < sizeof(uart_ports) / 4; i++)
	{
		if (uart_ports[i] == port)
			return 1;
	}
	return 0;
}

/**********************************************************************/
/** Main Function.
 *
 *  \param argc         number of arguments
 *  \param argv         pointer to arguments
 *
 *  \return 0=ok, or -1 on error
 */
int main(int argc, char** argv)
{
	int fd;
	int i;
	int opt;
	int error = 0;
	char *devName = NULL;
	unsigned int mode = 0;
	unsigned int read = 0;
	unsigned int raw = 0;
	unsigned char value = 0;
	struct serial_struct ser_info;

	/* parse command line arguments */
	while ((opt = getopt(argc,argv,"d:m:rx")) != -1) {
		switch (opt) {
			case 'd':
				devName = optarg;
				break;
			case 'm':
				mode = atoi(optarg);
				break;
			case 'r':
				read = 1;
				break;
			case 'x':
				raw = 1;
				break;
			default:
				usage();
				return 1;
		}
	}

	if (argc <= 3 ) {
		usage();
		return -1;
	}

	if (raw && !read) {
		usage();
		return -1;
	}

	/* Only set mode or read mode possible */
	if (mode && read) {
		usage();
		return -1;
	}

	/* Get access to IO ports */
	if (iopl(3)) {
		printf("Cannot get access to IO Ports\n");
		return -1;
	}

	fd = open( devName, O_RDWR);

	if (fd == -1) {
		perror("Cannot open tty port");
		return -1;
	}

	if (ioctl (fd,TIOCGSERIAL, &ser_info)) {
		perror("Cannot read serial info from device");
		error = -1;
		goto cleanup;
	}
	
	if (!validatePort(ser_info.port)) {
		perror("Port is not a valid UART port\n");
		error = -1;
		goto cleanup;
	}

	if (mode) {
		switch (mode) {
			case 1:
				printf("Set %s to RS232.\n",devName);
				outb(Z25_MODE_SE, ser_info.port + Z25_ACR);
				break;
				
			case 2:
				printf("Set %s to RS422/RS485 half duplex.\n",devName);
				outb(Z25_MODE_HDX, ser_info.port + Z25_ACR);
				break;

			case 3:
				printf("Set %s to RS422/RS485 full duplex.\n",devName);
				outb(Z25_MODE_FDX, ser_info.port + Z25_ACR);
				break;
				
			default:
				printf("Unknown Mode.\n");
				error = -1;
				goto cleanup;
		}
	}

	if (read){
		/* Read the UART Mode Bits */
		value = inb(ser_info.port + Z25_ACR) & 0x0f;
		
		/* RS232 */
		if ((value | Z25_MODE_SE) == Z25_MODE_SE)
			if (!raw)
				printf("Device: %s, Mode(1): RS232\n",devName);
			else
				printf("%d\n",SE_MODE);

		/* RS422/RS485 Full Duplex */
		else if ((value | Z25_MODE_FDX) == Z25_MODE_FDX)
			if (!raw)
				printf("Device: %s, Mode(3): RS422/RS485 full duplex\n",devName);
			else
				printf("%d\n",DIFF_FDX_MODE);

		/* RS422/RS485 Half Duplex */
		else if ((value | Z25_MODE_HDX) == Z25_MODE_HDX)
			if (!raw)
				printf("Device: %s, Mode(2): RS422/RS485 half duplex\n",devName);
			else
				printf("%d\n",DIFF_HDX_MODE);

		else
			printf("Device: %s, Unknown mode\n",devName);
	}


cleanup:
	close(fd);
	return error;
}


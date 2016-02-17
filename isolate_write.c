#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <getopt.h>

#define ARGS "s:rwc:irhn"

static struct option longopts[] = {
  {"isolate", required_argument, 0, 's'},
  {"read", no_argument, 0, 'r'},
  {"write", no_argument, 0, 'w'},
  {"content", required_argument, 0, 'c'},
  {"tag", required_argument, 0, 't'},
  {"inventory", no_argument, 0, 'i'},
  {"inventory_rssi", no_argument, 0, 'n'},
  {"help", no_argument, 0, 'h'},
  {0,0,0,0}
};

struct cmdln_args {
	char *tag;
	char *content;
	int write_flag;
	int read_flag;
	int arg_counter;
	int rssi_flag;
	int invent_flag;
} cmdln_args;


int set_interface_attribs (int fd, int speed, int parity)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
                printf ("error %d from tcgetattr", errno);
                return -1;
        }

        cfsetospeed (&tty, speed);
        cfsetispeed (&tty, speed);

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
        // disable IGNBRK for mismatched speed tests; otherwise receive break
        // as \000 chars
        tty.c_iflag &= ~IGNBRK;         // disable break processing
        tty.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
        tty.c_oflag = 0;                // no remapping, no delays
        tty.c_cc[VMIN]  = 0;            // read doesn't block
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

        tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                        // enable reading
        tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
        tty.c_cflag |= parity;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
        {
                printf ("error %d from tcsetattr", errno);
                return -1;
        }
        return 0;
}

void set_blocking (int fd, int should_block)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
                printf ("error %d from tggetattr", errno);
                return;
        }

        tty.c_cc[VMIN]  = should_block ? 1 : 0;
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
                printf ("error %d setting term attributes", errno);
}


int main(int argc, char ** argv)
{
	char *portname = "/dev/ttyUSB0";
	int i, j, k, n, ch, byte_length, switch_arg;
	int indexptr;
	int x = 0;	                                  // incrementer for timeout
	char buf [1000];
	char cmd[100];
	char id_buf[12];
	char hex_val_buf[10];
	int internal_arg_counter;
	int id_buf_index;
	int offset;
	int fd = open (portname, O_RDWR | O_NOCTTY | O_SYNC);
	char *val;

	// Error catch for open
	if (fd < 0) {
	  	printf ("error %d opening %s: %s", errno, portname, strerror (errno));
	  	exit(1);
	}

	set_interface_attribs (fd, B9600, 0); 		  // set speed to 9600 bps, 8n1 (no parity)
	set_blocking (fd, 0);               		  // set no blocking


	// Error catch for no arguments 
	if (argc < 2) {
		printf("No command given, exiting program.\n");
	  	printf("Usage: antenna [OPTIONS]\n");
		printf("Options:\n");
		printf("	-r ... read command\n");
		printf("	-w ... write command\n");
		printf("	-t tag_id 	 tag to read and write to\n");
		printf("	-c content	 content written to tag if write option activated\n");
		printf("	-i ... inventory command\n");
		printf("	-n ... inventory rssi command\n\n\n");
		printf("Notes:\n");
		printf("	Write command is as follows: antenna -w -t tag_id -c value_to_be_written\n");
		printf("	Read command is as follows: antenna -r -t tag_id\n"); 
	  	exit(1);
	}
	
	// Set default values for the cmdln_arg struct components
	cmdln_args.tag = NULL;
	cmdln_args.content = NULL;
	cmdln_args.read_flag = 0;
	cmdln_args.write_flag = 0;
	cmdln_args.arg_counter = 0;
	cmdln_args.rssi_flag = 0;
	cmdln_args.invent_flag = 0;
	


	while((ch = getopt_long(argc, argv, ARGS, longopts, &indexptr)) != -1) {
		switch(ch) {
			case 'r':
				//printf("Entered read command\n");
				cmdln_args.read_flag = 1;
				cmdln_args.arg_counter+=3;
				break;
			case 'w':
				//printf("Entered write command\n");
				cmdln_args.write_flag = 1;
				cmdln_args.arg_counter+=3;
				break;
			case 't':
				// Not going to need isolate command, will embed with the read and write commands
				//printf("Entered tag command\n");
				cmdln_args.tag = optarg;
				cmdln_args.arg_counter+=2;
				break;
			case 'c':
				//printf("Entered content command\n");
				cmdln_args.tag = optarg;
				cmdln_args.arg_counter++;
				break;
		  	case 'i':
				//printf("Entered inventory command\n");
				// Fill the buffer accordingly
				cmdln_args.invent_flag = 1;
				cmdln_args.arg_counter++;
				break;
		  	case 'n':
				//printf("Entered the inventory rssi command\n");
				// Fill buffer with RSSI inventory hex
				cmdln_args.rssi_flag = 1;
				cmdln_args.arg_counter++;
				break;
			case 'h':
				printf("Entered help command\n");
				printf("Usage: antenna [OPTIONS]\n");
				printf("Options:\n");
				printf("	-r ... read command\n");
				printf("	-w ... write command\n");
				printf("	-c content	 content written to tag if write option activated\n");
				printf("	-n ... inventory rssi command\n\n\n");
				printf("Notes:\n");
				printf("	Write command is as follows: antenna -w -c value_to_be_written\n"); 
				exit(0);
				break;
		}
	}
		

	// Error checking, cannot have both rssi inventory and read and writes at the same time
	// 	one of the functions is cancelled in this part
	if ((cmdln_args.rssi_flag !=0 || cmdln_args.invent_flag !=0) && (cmdln_args.read_flag != 0 || cmdln_args.write_flag != 0)) {
		printf("Must choose either inventory commands or read and write, not both\n");
		exit(0);
	}
	
	// If the write or read flag is raised, the inventory RSSI command must be issued first
	// then the read and writes will be able to work
	internal_arg_counter = 0;
	offset = 10;
	
	if (cmdln_args.read_flag != 0 || cmdln_args.write_flag != 0) {
			// Send the RSSI inventory cmd first
		if (internal_arg_counter == 0) {
			bzero(cmd, 100);
			bzero(buf, 1000);
			bzero(id_buf, 12);
			
			cmd[0] = 0x43;
			cmd[1] = 0x03;
			cmd[2] = 0x01;
			write (fd, cmd, 3);
			
			while (1) {
				//printf("In loop\n");
				n = read(fd, buf, sizeof buf);
				if (n > 0) {
					for (j = 0; j < n; j++) {
						//printf("0x%.2x ", buf[j]);
					}
					//printf("\n");
				}	
				sleep(1);
				if (n==0) {
					x+=1;
					break;
				}
				if (x==4) {
					x = 0;
					break;
				}
			}		
			// Will fill in the buffer with the RFID of the tag
 			strncpy(id_buf, buf+9, 12);
 			
 			/*
 			for (k = 0; k < 12; k++) {
 				printf("id_buf[%d] = %x\n", k, id_buf[k]);
 			}
 			*/
 			internal_arg_counter += 1;
		}
			
		// Will then send the isolate command second
		if (internal_arg_counter == 1) {
			bzero(cmd, 100);
			bzero(buf,1000);
			cmd[0] = 0x33;
			cmd[1] = 0x0F;
			cmd[2] = 0x0C;
			strncpy(cmd+3, id_buf, 12);
			
			write(fd, cmd, sizeof cmd);
			while (1) {
				n = read(fd, buf, sizeof buf);
				if (n > 0) {
					for (j = 0; j < n; j++) {
						//printf("0x%.2x ", buf[j]);
					}
					//printf("\n");
				}	
				sleep(1);
				if (n==0) {
					x+=1;
					break;
				}
				if (x==4) {
					x = 0;
					break;
				}
			}
			internal_arg_counter += 1;	
		}
		
		if (internal_arg_counter == 2) {
			bzero(cmd, 100);
			bzero(buf, 1000);
			if (cmdln_args.read_flag != 0) {
				cmd[0] = 0x37;
				cmd[1] = 0x05;
				cmd[2] = 0x01;
				cmd[3] = 0x02;
				cmd[4] = 0x06;
				write(fd, cmd, 5);
				while(1) {
					//printf("In loop\n");
					n = read(fd, buf, sizeof buf);
					if (n > 0) {
						for (j = 0; j < n; j++) {
							printf("0x%.2x ", buf[j]);
						}
						printf("\n");
					}	
					sleep(1);
					if (n==0) {
						x+=1;
						break;
					}
					if (x==4) {
						x = 0;
						break;
					}
				}
			
			}
			
			if (cmdln_args.write_flag != 0) {
				//printf("In write statement\n");
				// Check if there is data to be written to the tag
				if (cmdln_args.tag == NULL) {
					printf("No data is given to write to tag, exiting\n");
					exit(1);
				}
				bzero(cmd, 100);
				bzero(buf, 1000);
				cmd[0] = 0x35;
				cmd[1] = 0x15;
				cmd[2] = 0x01;
				cmd[3] = 0x02;
				// EPC password
				cmd[4] = 0x00;
				cmd[5] = 0x00;
				cmd[6] = 0x00;
				cmd[7] = 0x00;
				// Number of words written
				cmd[8] = 0xC0;
				
				//printf("%s\n", cmdln_args.tag);
				//printf("Length of string: %d\n", strlen(cmdln_args.tag));
				
				if (strlen(cmdln_args.tag) % 2 == 0) {
					//printf("In even\n");
					cmd[8] = strlen(cmdln_args.tag) / 2;
				}
				
				if (strlen(cmdln_args.tag) % 2 == 1) {
					//printf("In odd\n");
					cmd[8] = (strlen(cmdln_args.tag) + 1) / 2;
				}
				
				//printf("cmd[8] = %x\n", cmd[8]);
				
				
				// If there is data to be written, parse the char and input into cmd buffer
				strncpy(cmd+9, strtok(cmdln_args.tag, " "), 1);
				while ((val = strtok(NULL, " ")) != NULL) {
					strncpy(cmd+offset, val, 1);
					offset++;
				}
				
				//printf("For write command: \n");
				
				/*
				for (i = 0; i < 30; i++) {
					printf("cmd[%d] = %x\n", i, cmd[i]);
				}
				
				*/
				write(fd, cmd, sizeof cmd);
				while (1) {
					n = read(fd, buf, sizeof buf);
					if (n > 0) {
						for (j = 0; j < n; j++) {
							//printf("0x%.2x ", buf[j]);
						}
						//printf("\n");
					}	
					sleep(1);
					if (n==0) {
						x+=1;
						break;
					}
					if (x==4) {
						x = 0;
						break;
					}
				}
				for (i = 0; i < strlen(buf); i++) {
					if (buf[i] == 0x06) {
						printf("True\n");
						exit(0);
					}
				}
				printf("False\n");
			}
		}	
	}	
		
	if (cmdln_args.rssi_flag != 0) {
		bzero(cmd, 100);
		bzero(buf, 1000);
		cmd[0] = 0x43;
		cmd[1] = 0x03;
		cmd[2] = 0x01;
		write (fd, cmd, 3);
		while (1) {
			n = read(fd, buf, sizeof buf);
			if (n > 0) {
				for (j = 0; j < n; j++) {
					printf("0x%.2x ", buf[j]);
				}
				printf("\n");
			}
			sleep(1);
			if (n==0) {
				x+=1;
				break;
			}
			if (x==4) {
				x = 0;
				break;
			}
		}	
	}		

	
}
/*!															   www.weaved.com			
 *---------------------------------------------------------------------------
 *! \file  server_channel.c
 *  \brief Server Channel Handler/Daemon.
 *																			
 *---------------------------------------------------------------------------
 * Version                                                                  -
 *		0.1 Original Version January 20 2015							-        
 *
 *---------------------------------------------------------------------------    
 *                                                             				-
 * Copyright (C) 2015, Weaved Inc, www.weaved.com								-
 *                                                                         	-
 * $Date: mwj 2015/01/20 20:35:55 $
 *
 *---------------------------------------------------------------------------
 *
 * Notes:
 *
 *
 *
*/
#include "mytypes.h"
#include "config.h"
#include "debug.h"
#include "webio.h"
#include "arch.h"
#include "debug.h"
#include "net.h"
#include "log.h"
#include "yselect.h"
#include "daemonize.h"
#include "server_channel.h"
#include "server_channel_packet.h"

#if defined(WIN32)
#include "wingetopt.h"
#endif


#define	VERSION		"0.1"

// Globals
int             go=0;
SC_CONFIG      *global_sc_ptr;
U8				global_flag		=0;


#if defined(WIN32)
//
// Windows handler if you need to handle CTRL-C or other interrupts
//
BOOL WINAPI ConsoleHandler(DWORD CEvent)
{
    switch(CEvent)
    {
    case CTRL_C_EVENT:
		yprintf("CTRL+C received!\n");
        break;
    case CTRL_BREAK_EVENT:
		yprintf("CTRL+BREAK received!\n");
        break;
    case CTRL_CLOSE_EVENT:
		yprintf("program is being closed received!\n");
        break;
    case CTRL_SHUTDOWN_EVENT:
		yprintf("machine is being shutdown!\n");
		break;
    case CTRL_LOGOFF_EVENT:
		return FALSE;
    }
	go=0;

    return TRUE;
}

#else
//
// Unix handler if you need to handle CTRL-C or other signals
//
void
termination_handler (int signum)
{
	if(global_sc_ptr->verbose) printf("term handler for signal %d\n",signum);

	sprintf(global_sc_ptr->last_msg,"term handler for signal %d\n",signum);
	//write_statistics(global_chat_ptr);
	go=0;	

    if((SIGFPE==signum) || (SIGSEGV==signum) || (11==signum))
    {
        yprintf("Weaved DNS Terminated from Signal %d\n",signum);
		if(global_flag&GF_DAEMON) syslog(LOG_ERR,"Weaved DNS Terminated from Signal 11\n");

#if defined(BACKTRACE_SYMBOLS)
              {
                // addr2line?                
                void* callstack[128];
                int i, frames = backtrace(callstack, 128);
                char** strs = backtrace_symbols(callstack, frames);
                yprintf("backtrace:\n");
                for (i = 0; i < frames; ++i) 
                {
                    yprintf("T->%s\n", strs[i]);
                    if(global_flag&GF_DAEMON)  syslog(LOG_ERR,"T->%s\n", strs[i]);
                }
                free(strs);
                fflush(stdout);
              }
#endif
        exit(11);
    }
}
#endif

//
// Banner for Software
//
void
startup_banner()
{
	//------------------------------------------------------------------
	// Print Banner
	//------------------------------------------------------------------
	printf("server_channel_handler built " __DATE__ " at " __TIME__ "\n");
	printf("   Version " VERSION " - (c)2015 Weaved Inc. All Rights Reserved\n");
	fflush(stdout);	
}

//
// Usage for Software
//
void usage(int argc, char **argv)
{
  startup_banner();

  printf("usage: %s [-h] [-v(erbose)] [-d][pid file] [-f config_file]  \n",argv[0]);
  printf("\t -h this output.\n");
  printf("\t -v console debug output.\n");
  printf("\t -d runs the program as a daemon with optional pid file.\n");
  printf("\t -f specify a config file.\n");
  exit(2);
}


int 
write_sc_statistics(SC_CONFIG *sc)
{
    // Write statistics here
    return 0;
}



//
// Main Entryp point for software
//
int main(int argc, char *argv[])
{
//char			config_file[MAX_PATH];
SC_CONFIG       sc;
int				c;
U32				timestamp=second_count();

#if defined(LINUX) || defined(MACOSX)
// Only Unix can be run as daemon
/* Our process ID and Session ID */
//pid_t			pid, sid;

	signal(SIGPIPE, SIG_IGN);
#endif


	go=1;
    //
	// Set defaults
	// 
	//
	// Clean the whole SC Structure
	global_sc_ptr=&sc;
	memset(&sc,'\0',sizeof(SC_CONFIG));
	//
	// Set SC Defaults, can be overwritten later by config file, but will operate on these if it does not exist
	//
	sc.udp_listen_port=SERVER_CHANNEL_PORT_DEFAULT;
    sc.Bind_IP.ip32=inet_addr("127.0.0.1");                            // Localhost by default
    sc.verbose=0;
	//
	// Set update server and filter files
	//
	//default stats updated once every 60 seconds
    //
	strcpy(sc.stats_file,DEFAULT_STATS_FILE);
    sc.stats_interval=DEFAULT_STATISTICS_INTERVAL;

	//
	// Banner
	startup_banner();
	
	// Startup Network
	network_init();
    Y_Init_Select();



	//------------------------------------------------------------------
	// Initialize error handling and signals
	//------------------------------------------------------------------
#if defined(WIN32) 
if (SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler,TRUE)==FALSE)
{
    // unable to install handler... 
    // display message to the user
    yprintf("Error - Unable to install control handler!\n");
    exit(0);
}
#else 
#if !defined(WINCE)
	//	SetConsoleCtrlHandle(termination_handler,TRUE);

	if (signal (SIGINT, termination_handler) == SIG_IGN)
		signal (SIGINT, SIG_IGN);
	if (signal (SIGTERM, termination_handler) == SIG_IGN)
		signal (SIGTERM, SIG_IGN);
	if (signal (SIGILL , termination_handler) == SIG_IGN)
		signal (SIGILL , SIG_IGN);
	if (signal (SIGFPE , termination_handler) == SIG_IGN)
		signal (SIGFPE , SIG_IGN);
	//if (signal (SIGSEGV , termination_handler) == SIG_IGN)
	//	signal (SIGSEGV , SIG_IGN);
#if defined(LINUX) || defined(MACOSX) || defined(IOS)
	if (signal (SIGXCPU , termination_handler) == SIG_IGN)
		signal (SIGXCPU , SIG_IGN);
	if (signal (SIGXFSZ , termination_handler) == SIG_IGN)
		signal (SIGXFSZ , SIG_IGN);
#endif
#endif
#endif

	//
	//
	//
	while ((c = getopt(argc, argv, "u:l:dvh")) != EOF)
	{
    		switch (c) 
			{
    		case 0:
    			break;
    		case 'l':
    		    //log level
    		    sc.log_level = atoi(optarg);
    		    break;
            case 'u':
    		    //udp Port
    		    sc.udp_listen_port = atoi(optarg);
    		    break;
    		case 'd':
				// Startup as daemon with pid file
				printf("Starting up as daemon\n");
				strncpy(sc.pidfile,optarg,MAX_PATH-1);
				//global_flag&GF_DAEMON
                //dns.
                //fe.log_level = atoi(optarg);
    			break;
    		case 'v':
    			sc.verbose=1;
    			break;
    		case 'h':
    			usage (argc,argv);
    			break;
    		default:
    			usage (argc,argv);
				break;
    	}
    }
	argc -= optind;
	argv += optind;
	
	//if (argc != 1)
	//	usage (argc,argv);

	// Read File Config  (not used for now)
/*
    if(strlen(config_file))
	{
		if(read_file_config(config_file, &sc))
		{
			if(sc.verbose) printf("Config File Loaded\n");
		}
		else
		{
			if(sc.verbose) printf("Config File Failed to Load\n");
		}
	}
*/

	//
	// Bind UD Socket istener
	sc.udp_listen_soc=udp_listener(sc.udp_listen_port,sc.Bind_IP);

	if(sc.udp_listen_soc!=SOCKET_ERROR)
	{
		if(sc.verbose) printf("Bound to UDP %d.%d.%d.%d.%d on socket %d\n",sc.Bind_IP.ipb1,sc.Bind_IP.ipb2,sc.Bind_IP.ipb3,sc.Bind_IP.ipb4,
                                                                                sc.udp_listen_port,sc.udp_listen_soc);
        // nonblock on sock
	    set_sock_nonblock(sc.udp_listen_soc);
        // Add to select
        Y_Set_Select_rx(sc.udp_listen_soc);
	}
	else
	{
		if(sc.verbose) printf("Failed to bind to %d, error %d cannot Startup\n",sc.udp_listen_port,get_last_error());
		perror("bind\n");
		//get_last_error();
		go=0;
        exit(1);
	}

#if !defined(WIN32)
    //
    // Should Daemonize here if set
    //
	if(global_flag&GF_DAEMON)
	{
            // Daemonize this
            daemonize(sc.pidfile,0,0,0,0,0,0);

            // Setup logging
			openlog("server_channel",LOG_PID|LOG_CONS,LOG_USER);
			syslog(LOG_INFO,"Weaved Server Channel built "__DATE__ " at " __TIME__ "\n");
			syslog(LOG_INFO,"   Version " VERSION " - (c)2015 Weaved Inc. All Rights Reserved\n");
			syslog(LOG_INFO,"Starting up as daemon\n");
	       
    }
#endif

	//------------------------------------------------------------------
	// Initialize error handling and signals
	//------------------------------------------------------------------
#if defined(WIN32) 
if (SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler,TRUE)==FALSE)
{
    // unable to install handler... 
    // display message to the user
    yprintf("!!Error - Unable to install control handler!\n");
    return -1;
}
#else 
	if (signal (SIGINT, termination_handler) == SIG_IGN)
		signal (SIGINT, SIG_IGN);
	if (signal (SIGTERM, termination_handler) == SIG_IGN)
		signal (SIGTERM, SIG_IGN);
	if (signal (SIGILL , termination_handler) == SIG_IGN)
		signal (SIGILL , SIG_IGN);
	if (signal (SIGFPE , termination_handler) == SIG_IGN)
		signal (SIGFPE , SIG_IGN);
	if (signal (SIGSEGV , termination_handler) == SIG_IGN)
		signal (SIGSEGV , SIG_IGN);
	if (signal (SIGXCPU , termination_handler) == SIG_IGN)
		signal (SIGXCPU , SIG_IGN);
	if (signal (SIGXFSZ , termination_handler) == SIG_IGN)
		signal (SIGXFSZ , SIG_IGN);
#endif



    //---------------------------------------------------------------------------------------------------------------------
	// Main Loop Forever, we should exit on program termination, timeout every 1s if no packet to handle housekeeping      -
    //---------------------------------------------------------------------------------------------------------------------
	if(sc.verbose) printf("Starting Server Channel\n");	
	
    go=10;
    while(go)
	{
		// Everything fun happens in rx_packet, this is the server
		server_channel_rx(&sc,60);
		//
		// Do Checks and write statistics every 60 seconds, we also check for reload
        // Do not do this to fast on a heavy loaded server with lots of records stored in file,
        // reloads can cost.  Best to use control interface for dynamic server.
		//
		if((second_count()-timestamp)> sc.stats_interval)
		{
			//if(dns.verbose) printf("Try Reload\n");
			timestamp=second_count();	
            //
            // Do any other periodic timer stuff here
            //

			//
			// Write out statistics
			//
			write_sc_statistics(&sc);
			fflush(stdout);	
		}
	}

    // We are out of here, cleanup


	// Should never exit, but if we do cleanup and print statistics
	if(sc.verbose) printf("Exiting On Go = 0\n");	
	fflush(stdout);	
	
	return(0);
}



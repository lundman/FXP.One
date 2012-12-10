// $Id: settings.c,v 1.8 2012/02/29 06:44:48 lundman Exp $
//
// Jorgen Lundman 10th October 2003.
//
// Server Wide settings, read off disk.
//


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <getopt.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include "lion.h"
#include "misc.h"

#include "settings.h"
#include "debug.h"
#include "parser.h"



static parser_command_list_t settings_command_list[] = {
	{ COMMAND( "CONF" ), settings_conf },
	{ NULL,  0         ,         NULL  }
};


settings_t settings_values;

int settings_initialising = 0;


/*
 * Check if ~/.FXP.One/ exists, create if not
 * .. if mkdir, AND cwd has .FXP.One.sites, copy them over to default.
 * Set CWD based on OS. (eg: ~/.FXP.One/)
 *   .. unless otherwise over-ruled by arguments.
 * Open and read configuration file
 * Parse arguments to over-rule settings file
 *
 */




void settings_init(void)
{
	lion_t *conf_file;


	settings_defaults();

	settings_initialising = 1;

	// Using lion to read a config file, well, well...
	conf_file = lion_open(".FXP.One.settings", O_RDONLY, 0600,
						  LION_FLAG_NONE, NULL);


	if (conf_file) lion_set_handler(conf_file, settings_handler);
	else settings_save();

}



//
// userinput for reading the config.
//

int settings_handler(lion_t *handle, void *user_data,
					 int status, int size, char *line)
{

	switch(status) {

	case LION_FILE_OPEN:
		debugf("settings: reading settings file...\n");
		break;


	case LION_FILE_FAILED:
	case LION_FILE_CLOSED:
		debugf("settings: finished reading settings.\n");
		settings_initialising = 0;
		break;

	case LION_INPUT:
		//debugf("settings: read '%s'\n", line);

		parser_command(settings_command_list, line, NULL);

		break;

	}

	return 0;

}








void settings_conf(char **keys, char **values,
				   int items,void *optarg)
{
	char *value;

	if ((value = parser_findkey(keys, values, items, "VERSION"))) {

		// Lets just warn them, but carry on.
		if (strcasecmp(value, "1.0")) {
			printf("settings: Unknown version in settings file '%s'.\n",
				   value);
		}

	}


	// Parse in command settings
	if ((value = parser_findkey(keys, values, items, "command_port")))
		settings_values.command_port = atoi(value);

	if((value = parser_findkey(keys, values, items, "command_ssl_only")))
		settings_values.command_ssl_only = atoi(value);

	if ((value = parser_findkey(keys, values, items, "command_iface")))
		settings_values.command_iface = lion_addr(value);

	if ((value = parser_findkey(keys, values, items, "http_port")))
		settings_values.http_port = atoi(value);

	if((value = parser_findkey(keys, values, items, "http_ssl_only")))
		settings_values.http_ssl_only = atoi(value);

	if ((value = parser_findkey(keys, values, items, "http_iface")))
		settings_values.http_iface = lion_addr(value);

}





void settings_save(void)
{

	lion_t *conf_file;

	// Using lion to read a config file, well, well...

	conf_file = lion_open(".FXP.One.settings", O_WRONLY|O_TRUNC|O_CREAT, 0600,
						  LION_FLAG_NONE, NULL);

	if (!conf_file) {
		printf("settings: failed to create settings file!\n");
		return;
	}

	// Write only, disable read or LiON thinks its at EOF
	lion_disable_read(conf_file);

	// Not needed but we don't want events in the main handlers.
	lion_set_handler(conf_file, settings_handler);

	lion_printf(conf_file, "# FXP.One settings file. Written automatically.\r\n");

	lion_printf(conf_file, "CONF|VERSION=1.0\r\n");

	// Command settings
	lion_printf(conf_file, "CONF|command_port=%d|command_ssl_only=%d",
				settings_values.command_port,
				(int)settings_values.command_ssl_only);
	lion_printf(conf_file, "|http_port=%d|http_ssl_only=%d",
				settings_values.http_port,
				(int)settings_values.http_ssl_only);

	if (settings_values.command_iface)
		lion_printf(conf_file, "|command_iface=%s",
					lion_ntoa(settings_values.command_iface));
	if (settings_values.http_iface)
		lion_printf(conf_file, "|http_iface=%s",
					lion_ntoa(settings_values.http_iface));

	lion_printf(conf_file, "\r\n");

	lion_close(conf_file);

	settings_initialising = 0;
}



void settings_defaults(void)
{
	struct stat stsb;

	memset(&settings_values, 0, sizeof(settings_values));

	// If 0 is not the default, set them here.
	// Do we have a cert loaded? If so, setting SSL only means nobody
	// could ever login.
	if (!stat(CERTFILE, &stsb)) {
		settings_values.command_ssl_only = YNA_YES;
		settings_values.http_ssl_only = YNA_YES;
    } else {
		settings_values.command_ssl_only = YNA_AUTO;
		settings_values.http_ssl_only = YNA_AUTO;
    }

	settings_values.command_port = 8885;
	settings_values.http_port    = 8886;

}


yesnoauto_t str2yna(char *s)
{
	if (!s || !*s) return YNA_AUTO;


	if (!mystrccmp("AUTO", s)) return YNA_AUTO;
	if (!mystrccmp("NO",   s)) return YNA_NO;
	if (!mystrccmp("YES",  s)) return YNA_YES;

	switch(atoi(s)) {
	case 0:
		return YNA_NO;
	case 1:
		return YNA_YES;
	default:
		return YNA_AUTO;
	}

	// NOT REACHED
	return YNA_AUTO;
}


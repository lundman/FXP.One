// $Id: sites.c,v 1.18 2010/10/12 02:36:32 lundman Exp $
//
// Jorgen Lundman 30th January 2004.
//
//
//


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lion.h"
#include "misc.h"

#include "sites.h"
#include "parser.h"
#include "debug.h"

//
// init   - read in the defined sites off disk
// free   - release sites from memory
// save   - save sites to disk
// setkey - set key used with loading/saving.
//
// commands:
// update - change/modify site
// add    - create new entry
// delete - delete entry
// list   - list sites

static char *sites_key = NULL;

static sites_t *sites_head = NULL;

static int sites_id = 0;

static lion_t *sites_file = NULL;


static parser_command_list_t sites_command_list[] = {
	{ COMMAND( "SITE" ),     sites_cmd_site },
	{ NULL,  0         ,          NULL }
};





void sites_setkey(char *key)
{

	if (key && !*key)
		key = NULL;

	SAFE_COPY(sites_key, key);

}








void sites_init(void)
{

	// Read the sites file into memory. This can be issued many times
	// to re-read it if there are changes.



	// Free any memory first incase we are to re-load the file.
	sites_free();

	sites_id = 0;

	sites_file = lion_open(SITES_CONF,
						   O_RDONLY,
						   0600,
						   LION_FLAG_NONE, NULL);


	if (sites_file) {

		lion_set_handler(sites_file, sites_handler);

		if (sites_key) {

			lion_ssl_set(sites_file, LION_SSL_FILE);

			lion_ssl_setkey(sites_file, sites_key, strlen(sites_key));

		}

	}


}



void sites_free(void)
{
	sites_t *runner, *next;

	for (runner = sites_head; runner; runner = next ) {

		next = runner->next;

		sites_freenode(runner);

	}

}









//
// userinput for reading the config.
//

int sites_handler(lion_t *handle, void *user_data,
				  int status, int size, char *line)
{

	switch(status) {

	case LION_FILE_OPEN:
		debugf("sites: reading sites file...\n");
		break;


	case LION_FILE_FAILED:
	case LION_FILE_CLOSED:
		debugf("sites: finished reading sites.\n");
		sites_file = NULL;
		break;

	case LION_INPUT: // un-encrypted
		//debugf("sites: read '%s'\n", line);
		parser_command(sites_command_list, line, NULL);

		break;

	case LION_BINARY: // Shouldn't happen
		break;


	}

	return 0;

}




void sites_save(void)
{

	sites_file = lion_open(SITES_CONF,
						   O_WRONLY|O_CREAT|O_TRUNC,
						   0600,
						   LION_FLAG_NONE, NULL);


	if (sites_file) {

		lion_set_handler(sites_file, sites_handler);

		// We are writing, don't try to read the file.
		lion_disable_read(sites_file);

		if (sites_key) {

			lion_ssl_set(sites_file, LION_SSL_FILE);

			lion_ssl_setkey(sites_file, sites_key, strlen(sites_key));

		}

		sites_listentries(sites_file, SITES_LIST_SAVE);



		// Write something to the file
		lion_close(sites_file);
		return;

	}

	printf("[sites]: Warning, failed to create sites file? Permissions? Disk full?\n");

}



void sites_removenode(sites_t *node)
{
	sites_t *runner, *prev;

	for (runner = sites_head, prev = NULL;
		 runner;
		 prev = runner, runner = runner->next) {

		if (runner == node) {

			if (!prev) {

				sites_head = node->next;

			} else {

				prev->next = node->next;

			}

			return; // found, just return

		}

	}

}

void sites_freenode(sites_t *node)
{
	int i;

	sites_removenode(node);

	SAFE_FREE(node->name);
	SAFE_FREE(node->user);
	SAFE_FREE(node->pass);
	SAFE_FREE(node->host);

	SAFE_FREE(node->file_skiplist);
	SAFE_FREE(node->directory_skiplist);
	SAFE_FREE(node->file_passlist);
	SAFE_FREE(node->directory_passlist);
	SAFE_FREE(node->file_movefirst);
	SAFE_FREE(node->directory_movefirst);

	if (node->items && node->keys && node->values) {

		for (i = 0; i < node->items; i++) {

			SAFE_FREE(node->keys[i]);
			SAFE_FREE(node->values[i]);

		}

		SAFE_FREE(node->keys);
		SAFE_FREE(node->values);
		node->items = 0;

	}

	SAFE_FREE(node);

}


sites_t *sites_newnode(void)
{
	sites_t *result;


	result = (sites_t *) malloc(sizeof(*result));

	if (!result) return NULL;


	memset(result, 0, sizeof(*result));


	// Set defaults.
	result->passive      = YNA_AUTO;
	result->fxp_passive  = YNA_AUTO;
	result->control_TLS  = YNA_AUTO;
	result->data_TLS     = YNA_AUTO;
	result->desired_type = YNA_AUTO;
	result->resume       = YNA_AUTO;
	result->resume_last  = YNA_AUTO;
	result->pret         = YNA_AUTO;
	result->file_skipempty      = YNA_AUTO;
	result->directory_skipempty = YNA_AUTO;
	result->port         = 21;

	return result;
}


void sites_insertnode(sites_t *node)
{

	if (!node) return;

	node->id = sites_id++;

	node->next = sites_head;
	sites_head = node;

}



//
// Take a argc/argv pair, parse out the required fields for a site
// then save any optional fields for GUIs.
//
// Return standard error codes, or 0 for success.
//
int sites_parsekeys(char **keys, char **values, int items, sites_t *node)
{
	int i, len;
	int xitems;
	unsigned long set = 0;  // make sure we have seen all required fields.

	xitems = items;

#ifdef DEBUG_VERBOSE
	debugf("[sites_parsekeys] parsing for %p\n", node);
#endif

	for (i = 0; i < items; i++) {

		// Empty key, just loop
		if (!keys[i]) continue;

		len = strlen(keys[i]);

#define tester(X) ((sizeof((X))-1 == len) && !strcasecmp(keys[i], (X)))

		// REQUIRED
		if (tester("name")) {
			if (!values[i]) return 1504; // Missing required field
			SAFE_COPY(node->name, values[i]);
			keys[i] = NULL;
			xitems--;
			set |= 1;
		} else if (tester("host")) {
			if (!values[i]) return 1504;
			SAFE_COPY(node->host, values[i]);
			keys[i] = NULL;
			xitems--;
			set |= 2;
		} else if (tester("user")) {
			if (!values[i]) return 1504;
			SAFE_COPY(node->user, values[i]);
			keys[i] = NULL;
			xitems--;
			set |= 4;
		} else if (tester("pass")) {
			if (!values[i]) return 1504;
			SAFE_COPY(node->pass, values[i]);
			keys[i] = NULL;
			xitems--;
			set |= 8;

			// OPTIONAL
		} else if (tester("port")) {
			if (!values[i]) continue;
			node->port = atoi(values[i]);
			keys[i] = NULL;
			xitems--;
		} else if (tester("passive")) {
			if (!values[i]) continue;
			node->passive = str2yna(values[i]);
			keys[i] = NULL;
			xitems--;
		} else if (tester("fxp_passive")) {
			if (!values[i]) continue;
			node->fxp_passive = str2yna(values[i]);
			keys[i] = NULL;
			xitems--;
		} else if (tester("control_TLS")) {
			if (!values[i]) continue;
			node->control_TLS = str2yna(values[i]);
			keys[i] = NULL;
			xitems--;
		} else if (tester("data_TLS")) {
			if (!values[i]) continue;
			node->data_TLS = str2yna(values[i]);
			keys[i] = NULL;
			xitems--;
		} else if (tester("iface")) {  // FIXME!! Use char *
			if (!values[i]) continue;
			node->iface = lion_addr(values[i]);
			keys[i] = NULL;
			xitems--;
		} else if (tester("iport")) {
			if (!values[i]) continue;
			node->iport = atoi(values[i]);
			keys[i] = NULL;
			xitems--;
		} else if (tester("desired_type")) {
			if (!values[i]) continue;
			node->desired_type = str2yna(values[i]);
			keys[i] = NULL;
			xitems--;
		} else if (tester("resume")) {
			if (!values[i]) continue;
			node->resume = str2yna(values[i]);
			keys[i] = NULL;
			xitems--;
		} else if (tester("resume_last")) {
			if (!values[i]) continue;
			node->resume_last = str2yna(values[i]);
			keys[i] = NULL;
			xitems--;
		} else if (tester("pret")) {
			if (!values[i]) continue;
			node->pret = str2yna(values[i]);
			keys[i] = NULL;
			xitems--;
		} else if (tester("fskiplist")) {
			if (!values[i]) {
				SAFE_FREE(node->file_skiplist);
				continue;
			}
			SAFE_COPY(node->file_skiplist, values[i]);
			keys[i] = NULL;
			xitems--;
		} else if (tester("dskiplist")) {
			if (!values[i]) {
				SAFE_FREE(node->directory_skiplist);
				continue;
			}
			SAFE_COPY(node->directory_skiplist, values[i]);
			keys[i] = NULL;
			xitems--;
		} else if (tester("fpasslist")) {
			if (!values[i]) {
				SAFE_FREE(node->file_passlist);
				continue;
			}
			SAFE_COPY(node->file_passlist, values[i]);
			keys[i] = NULL;
			xitems--;
		} else if (tester("dpasslist")) {
			if (!values[i]) {
				SAFE_FREE(node->directory_passlist);
				continue;
			}
			SAFE_COPY(node->directory_passlist, values[i]);
			keys[i] = NULL;
			xitems--;
		} else if (tester("fskipempty")) {
			if (!values[i]) continue;
			node->file_skipempty = str2yna(values[i]);
			keys[i] = NULL;
			xitems--;
		} else if (tester("dskipempty")) {
			if (!values[i]) continue;
			node->directory_skipempty = str2yna(values[i]);
			keys[i] = NULL;
			xitems--;
		} else if (tester("fmovefirst")) {
			if (!values[i]) {
				SAFE_FREE(node->file_movefirst);
				continue;
			}
			SAFE_COPY(node->file_movefirst, values[i]);
			keys[i] = NULL;
			xitems--;
		} else if (tester("dmovefirst")) {
			if (!values[i]) {
				SAFE_FREE(node->directory_movefirst);
				continue;
			}
			SAFE_COPY(node->directory_movefirst, values[i]);
			keys[i] = NULL;
			xitems--;


			// Parser adds the "type" field to say what command we were
			// so we had better take it out.
		} else if (tester("type")) {
			keys[i] = NULL;
			xitems--;
		} else if (tester("SITEID")) {
			keys[i] = NULL;
			xitems--;
		}


#undef tester

	}


#ifdef DEBUG_VERBOSE
	debugf("[sites_parsekeys] saw fields %lu.\n", set);
#endif

	// Check if we saw all required fields.
	// name, host, user and pass.

	// Save any left over fields
	// Subtract the number of matched fields, and allocate the arrays to
	// hold the remainding.

	if (xitems) {

#ifdef DEBUG_VERBOSE
		debugf("[sites] %d items remaining..\r\n", xitems);
#endif

		// We want to keep/copy the old key/pairs, and check against fields
		// given to remove some, or add new.

		// Find key/value to delete.
		if (node->items && node->keys) {
			for (i = 0; i < items; i++) {
				if (keys[i] && (!values[i] || !values[i][0])) {

					// Look for keys[i] in the site's keys. If it finds it
					// it sets it to NULL. (Delete). We also wipe it from
					// keys to not consider it as an addition.
					parser_findkey_once(node->keys, node->values,
										node->items,
										keys[i]);

					keys[i] = NULL;
					xitems--;
				}
			}
		}

		// All other remaining key/pairs are either changes, or, additions.
		// Do we need to allocate more space?
		if (xitems > node->items) {
			char **tmp;

			tmp  = (char **) realloc(node->keys,
									 sizeof(char *) * xitems);
			if (!tmp) return 401; // Out of memory
			node->keys = tmp;

			tmp  = (char **) realloc(node->values,
									 sizeof(char *) * xitems);
			if (!tmp) return 401; // Out of memory
			node->values = tmp;
		}

		//node->items = xitems;

		// Assign in...
		for (i = 0; i < items; i++) {
			int j, empty;

			if (keys[i] && values[i]) {

				// Still set, we should save it.
				// Attempt to find it, in node->keys
				for (j = 0, empty = -1;
					 j < node->items; j++) {

					// If a key position is empty, remember it
					if (!node->keys[j]) {
						if (empty < 0) empty = j;
						continue; // It's empty, don't bother comparing below
					}

					if (!strcasecmp(keys[i], node->keys[j])) {
						// Key exists, just copy over the new value.
						SAFE_DUPE(node->values[j], values[i]);
						break;
					}
				}

				// Didn't find it? If so add it.
				if (j >= node->items) {
					// Did we not find an empty slot?
					if (empty == -1) {
						empty = node->items;

						// Add more space.
						node->items++;
						char **tmp;

						tmp  = (char **) realloc(node->keys,
							sizeof(char *) * node->items);
						if (!tmp) return 401; // Out of memory
						node->keys = tmp;

						tmp  = (char **) realloc(node->values,
							sizeof(char *) * node->items);
						if (!tmp) return 401; // Out of memory
						node->values = tmp;
					}
					// Ok, there is space.

					SAFE_DUPE(node->keys[empty],   keys[i]);
					SAFE_DUPE(node->values[empty], values[i]);

				} // Added node

			} // both keys and values set.

		} // for all remaining items.

		// We could update node->items when there was deletion.. But they
		// should not be saved to disk if they are NULL, and so, next load
		// they are set correctly.


		// xitems should be 0 here, or something went wrong, but we
		// can just adjust out value and not panic about it.

	} // xitems





	// This check is only for siteadd, not sitemod
	if ((set & (15)) != 15)
		return 1504;





	return 0;

}




//
// Called from parser, from reading the sites file.
//
void sites_cmd_site(char **keys, char **values,
					int items,void *optarg)
{
	sites_t *newsite;
	int ret;

	// Fetch out required fields and assign left overs.
	newsite = sites_newnode();

	if (!newsite) {

		debugf("[sites_cmd_site] Out of memory reading sites\n");
		return;
	}


	// This checks for required fields, then saves optional.
	ret = sites_parsekeys(keys, values, items, newsite);

	if (ret) {
		debugf("[sites_cmd_site] sites file contains invalid entries: %d\n",
			   ret);
		sites_freenode(newsite);
		return;
	}

	// Chain it in
	sites_insertnode(newsite);

#ifdef DEBUG_VERBOSE
	debugf("[sites_cmd_site] read site %u:'%s'\n",
		   newsite->id, newsite->name);
#endif

}



void sites_listentries(lion_t *handle, int id)
{
	sites_t *runner;

	if ((id == SITES_LIST_ALL) ||
		(id == SITES_LIST_ALL_SHORT) ||
		(id == SITES_LIST_SAVE)) {

		// Write all users
		for (runner = sites_head; runner; runner = runner->next ) {

			if (runner->name) {

				// Save out the required fields first, then the optionals.
				if (id == SITES_LIST_ALL_SHORT) {
					lion_printf(handle,
								"SITELIST|SITEID=%u|NAME=%s\r\n",
								runner->id,
								runner->name);
				} else {
					sites_listentry(handle, runner, id);
				}
			} // if name

		} // for runner

	} else { // ALL || SAVE

		runner = sites_getid(id);

		if (runner)
			sites_listentry(handle, runner, id);

	}

}


void sites_listentry(lion_t *handle, sites_t *runner, int id)
{
	int i;

	// If we are in SAVE mode, we use differnt keyword and no ID sent.
	// Could be the same keyword, but wanted the seperation to avoid
	// confusion between keyword in sites file, and from connections via
	// command2.c


	if (id == SITES_LIST_SAVE)
		lion_printf(handle,
					"SITE|NAME=%s|HOST=%s|PORT=%u|USER=%s|PASS=%s|"
					"PASSIVE=%u|FXP_PASSIVE=%u|CONTROL_TLS=%u|"
					"DATA_TLS=%u",
					runner->name,
					runner->host ? runner->host : "",
					runner->port,
					runner->user ? runner->user : "",
					runner->pass ? runner->pass : "",
					runner->passive,
					runner->fxp_passive,
					runner->control_TLS,
					runner->data_TLS);
	else
		lion_printf(handle,
					"SITELIST|SITEID=%u|NAME=%s|HOST=%s|PORT=%u|USER=%s|PASS=%s|"
					"PASSIVE=%u|FXP_PASSIVE=%u|CONTROL_TLS=%u|"
					"DATA_TLS=%u",
					runner->id,
					runner->name,
					runner->host ? runner->host : "",
					runner->port,
					runner->user ? runner->user : "",
					runner->pass ? runner->pass : "",
					runner->passive,
					runner->fxp_passive,
					runner->control_TLS,
					runner->data_TLS);

	// Extended attributes
	if (runner->iface)
		lion_printf(handle, "|IFACE=%s", lion_ntoa(runner->iface));
	if (runner->iport)
		lion_printf(handle, "|IPORT=%u", runner->iport);
	if (runner->desired_type != YNA_AUTO)
		lion_printf(handle, "|DESIRED_TYPE=%u", runner->desired_type);
	if (runner->resume != YNA_AUTO)
		lion_printf(handle, "|RESUME=%u", runner->resume);
	if (runner->resume_last != YNA_AUTO)
		lion_printf(handle, "|RESUME_LAST=%u", runner->resume_last);
	if (runner->pret != YNA_AUTO)
		lion_printf(handle, "|PRET=%u", runner->pret);
	if (runner->file_skiplist)
		lion_printf(handle, "|FSKIPLIST=%s", runner->file_skiplist);
	if (runner->directory_skiplist)
		lion_printf(handle, "|DSKIPLIST=%s", runner->directory_skiplist);
	if (runner->file_passlist)
		lion_printf(handle, "|FPASSLIST=%s", runner->file_passlist);
	if (runner->directory_passlist)
		lion_printf(handle, "|DPASSLIST=%s", runner->directory_passlist);
	if (runner->file_skipempty != YNA_AUTO)
		lion_printf(handle, "|FSKIPEMPTY=%u", runner->file_skipempty);
	if (runner->directory_skipempty != YNA_AUTO)
		lion_printf(handle, "|DSKIPEMPTY=%u", runner->directory_skipempty);
	if (runner->file_movefirst)
		lion_printf(handle, "|FMOVEFIRST=%s", runner->file_movefirst);
	if (runner->directory_movefirst)
		lion_printf(handle, "|DMOVEFIRST=%s", runner->directory_movefirst);


	// Any optional fields?
	if (runner->items && runner->keys && runner->values) {

		for (i = 0; i < runner->items; i++) {

			// We have to have keys, values is optional
			if (!runner->keys[i]) continue;

			lion_printf(handle,
						"|%s=%s",
						runner->keys[i],
						runner->values[i] ? runner->values[i] :"");
		} // for items
	} // if items

	lion_printf(handle, "\r\n");

}



sites_t *sites_getid(int id)
{
	sites_t *runner;

	for (runner = sites_head; runner; runner = runner->next ) {

		if (runner->id == id) return runner;

	}

	return NULL;

}



void sites_del(sites_t *site)
{

	SAFE_FREE(site->name);

	sites_save();

}



char *sites_find_extra(sites_t *site, char *key)
{
	char *value;

	if (!site || !site->items || !site->keys) return NULL;

	value = parser_findkey(site->keys, site->values, site->items, key);

	return value;
}

int sites_iodone(void)
{

	if (sites_file) return 0;

	return 1;
}

char *site_get_listargs(sites_t *site)
{
    char *value;
    value = sites_find_extra(site, "LISTARGS");
    if (!value) return DEFAULT_LISTARGS;
    return value;
}

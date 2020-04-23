/***********************************************************
   Copyright 2020 Tallac Networks, Inc.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <jansson.h>
#include <time.h>
#include <getopt.h>

#include "lldp-sim.h"
#include "ev-sim.h"
#include "topo.h"

	
node_t *
get_node(node_t *n, int id)
{
    while(n) {
        if (id == n->id) 
            return n;
        else
            n = n->next;
    }
    return NULL;
}

int 
add_link(node_t *s, node_t *d)
{
    port_t      *s_port, *d_port = NULL;
    link_t      *link = NULL;

    if (s->port_ct >= MAX_BRIDGE_PORTS) {
        printf("Max ports reached, unsupported topology\n");
        return -1;
    }

    link = calloc(1, sizeof(link_t));

    s_port = &s->ports[s->port_ct];
    d_port = &d->ports[d->port_ct];

    s_port->link = link;
    d_port->link = link;

    s_port->next = d_port;
    d_port->next = NULL;
    link->peer_list = s_port;
    link->peer_ct = 2;

    s_port->id = ++s->port_ct;
    d_port->id = ++d->port_ct;

    s_port->node = (void *)s;
    d_port->node = (void *)d;

    if (s->node_type == N_BRIDGE) 
        s_port->pattr = P_UNKNOWN;
    else
        s_port->pattr = P_UPLINK;

    if (d->node_type == N_BRIDGE) 
        d_port->pattr = P_UNKNOWN;
    else
        d_port->pattr = P_UPLINK;

    s_port->my_last_pattr = s_port->pattr;
    d_port->my_last_pattr = d_port->pattr;
}

int
make_connections(json_t *js_obj, node_t *n)
{
	json_t	*js_array, *js_conn, *js_dest_node, *js_source_node;
    int i, num_conns = 0;
	char dest_id[12], source_id[12]; 
    int d_id, s_id;
    int err = -1;
    node_t  *s_node, *d_node = NULL;

	do {
		js_array = json_object_get(js_obj, "connections");
		if (!js_array) {
			perror("No connections object\n");
			break;
		}

		num_conns = json_array_size(js_array);
		for (i=0; i<num_conns; ++i) {
			js_conn = json_array_get(js_array, i);
			if (json_is_object(js_conn)) {

				js_dest_node = json_object_get(js_conn, "destination_id");
				if(!json_is_string(js_dest_node)) {
				    printf("bad dest node\n");
				    break;
				}
				strncpy(dest_id, json_string_value(js_dest_node), sizeof(dest_id));
				if (strlen(dest_id) < 5) {
				    printf("Node ID %s too short\n", dest_id);
                    break;
                }
                int d_id = atoi(&dest_id[4]);

				js_source_node = json_object_get(js_conn, "source_id");
				if(!json_is_string(js_source_node)) {
				    printf("bad source node\n");
				    break;
				}
				strncpy(source_id, json_string_value(js_source_node), sizeof(source_id));
				if (strlen(source_id) < 5) {
				    printf("Node ID %s too short\n", source_id);
                    break;
                }
                int s_id = atoi(&source_id[4]);

                s_node = get_node(n, s_id);
                d_node = get_node(n, d_id);
    
                if ((s_node == NULL) || (d_node == NULL)) {
                    printf("A node wasn't found\n");
                    break;
                }

                if (add_link(s_node, d_node) < 0) {
                    printf("Error adding link between nodes\n");
                    break;
                }
            }
        }
        err = 0;
    } while(0);

    return err;
}

int
add_shared_links(void)
{
    int i, rn, rp, ct;
    node_t *n, *r_node;
    link_t *r_link;
    port_t *r_port;
    int skip = 0;

    if (shared_links <= 0)
        return 0;

    if (shared_links >= MAX_BRIDGE_PORTS) {
        printf("Too many shared links\n");
        exit(1);
    }

    /* Build a bridge node to use for shared links */

    n = calloc(1, sizeof(node_t));
    if (!n) {
        printf("Calloc error\n");
        exit(1);
    }
    ++num_nodes;
    n->id = num_nodes;
    n->node_type = N_BRIDGE;
    n->level = -1;
    n->port_ct = shared_links;
    n->next = nodes;
    nodes = n;

    for (i=0; i<shared_links; ++i) {
        port_t *p = &n->ports[i];
        p->id = i+1;
        p->node = n;
        p->pattr = P_UNKNOWN;

        ct = 0;
        do {
            ++ct;

            /* get a random node, but not the new one */
            rn = rand() % num_nodes;
            r_node = get_node(nodes, rn);
            if (!r_node) {
                printf("Can't find node id %d\n", rn);
                exit(1);
            }

            /* get a random shared link */
            rp = rand() % r_node->port_ct;
            r_port = &r_node->ports[rp];
            r_link = (link_t *)r_port->link;

            /* Make sure this port isn't already on the shared link */
            r_port = r_link->peer_list;
            skip = 0;
            while(r_port) {
                if (r_port == p) {
                    skip = 1;
                    break;
                } else
                    r_port  = r_port->next;
            }

        } while ((skip) && (ct < 2*num_nodes));
         
        if (skip) {
            printf("Can't find a shared link to use\n");
            exit(1);
        }

        /* Add this port to the shared link */
        p->link = r_link;
        p->next = r_link->peer_list;
        r_link->peer_list = p;
        ++r_link->peer_ct;
    }


}

int
get_nodes(json_t *js_obj, node_t **n)
{
	json_t	*js_array;
	json_t	*js_node, *js_node_type, *js_node_id;
	char node_type[8]; 
	char node_id[12]; 
	int i;
	int err = -1;

	do {

		js_array = json_object_get(js_obj, "nodes");
		if (!js_array) {
			perror("No nodes object\n");
			break;
		}

		num_nodes = json_array_size(js_array);
		for (i=0; i<num_nodes; ++i) {
			js_node = json_array_get(js_array, i);
			if (json_is_object(js_node)) {

				js_node_type = json_object_get(js_node, "node_type");
				if(!json_is_string(js_node_type)) {
				    printf("bad node_type\n");
				    break;
				}
				strncpy(node_type, json_string_value(js_node_type), sizeof(node_type));

				js_node_id = json_object_get(js_node, "node_id");
				if(!json_is_string(js_node_id)) {
				    printf("bad node_id\n");
				    break;
				}
				strncpy(node_id, json_string_value(js_node_id), sizeof(node_id));
				if (strlen(node_id) < 5) {
				    printf("Node ID %s too short\n", node_id);
                    break;
                }
                int id = atoi(&node_id[4]);

				// Create node and link it

                node_t *nn = calloc(1, sizeof(node_t));
                if (!nn) {
                    printf("Calloc error\n");
                    break;
                }

				if (strcmp(node_type, "router") == 0) {
                    nn->node_type = N_BRIDGE;
                    nn->level = -1;
				} else if (strcmp(node_type, "server") == 0) {
                    nn->node_type = N_SERVER;
                    nn->level = 0;
				} else {
					printf("Invalid node_type\n");
					break;
				}
                nn->id = id;
                nn->my_last_node_type = nn->node_type;
                nn->my_last_level = nn->level;
                nn->next = *n;
                *n = nn;
			}
		} 

		err = 0;
	} while(0);

	return(err);
}

void
dump_nodes(node_t *n)
{
    node_t *nn, *p_node;
    port_t *p;
    link_t *l;
    int i;

    printf("\n");
    nn = n;
    while (nn) {
        printf("%s n%d, level %d, port count %d", nn->node_type == N_BRIDGE ? "Bridge " : "Server ", 
                nn->id, nn->level, nn->port_ct);
        for (i=0; i<nn->port_ct; ++i) {
            p = &nn->ports[i];
            l = (link_t *)p->link;
            if (i==0) printf("\n");
            if (!l) {
                printf("No link\n");
                return;
            }
            p = l->peer_list;
            while(p) {
                p_node = (node_t *)p->node;
                printf("  n%d-p%d,%d ", p_node->id, p->id, p->pattr);
                p = p->next;
            }
            printf("\n");
        }
        printf("\n\n");
        nn = nn->next;
    }
}

void
system_start(node_t *n)
{
    node_t  *n_peer;
    port_t  *p_peer;
    port_t  *p;
    link_t  *p_link;
    int     i;

#ifdef DEBUG_OUTPUT
    printf("%d: starting %s n%d and %d ports\n", ev_timenow(),
            n->node_type == N_BRIDGE ? "Router" : "Server", n->id, n->port_ct);
#endif

    n->status = 1;

    for (i=0; i<n->port_ct; ++i) {
        p = &n->ports[i];
        p->status = 1;
        p->tx_fast = LLDP_MAX_FAST;
        p->tx_credit = LLDP_MAX_TX_CREDIT;
        lldp_xmit(n, p, rand() % jitter, NULL);
    }
}

void
start_nodes(node_t *n_head)
{
    node_t  *n = n_head;
    event_t *ev;
    int st;

    while(n) {
        st = rand() % start_delay;           // bring up at random times
        ev = calloc(1, sizeof(event_t));
        ev->ev_type = EV_SYS_START;
        ev->ev_time = ev_timenow() + st;
        ev->ev_owner = n;
        ev_insert(ev);
        n = n->next;
    }
}

void
usage(const char *cmd)
{
    fprintf(stderr, "usage: %s\n", cmd);
    fprintf(stderr, "  %s -a num     ==> set algorithm number (defaults to 0)\n", cmd);
    fprintf(stderr, "  %s -d num     ==> set start-up delay max (random 0 to num)\n", cmd);
    fprintf(stderr, "  %s -f file    ==> topology configuratio file\n", cmd);
    fprintf(stderr, "  %s -h         ==> show help\n", cmd);
    fprintf(stderr, "  %s -j num     ==> set the random jitter on transmit\n", cmd);
    fprintf(stderr, "  %s -m num     ==> maximum simulation time in ticks\n", cmd);
    fprintf(stderr, "  %s -s num     ==> number of shared links\n", cmd);

    exit(1);
}

const char *optlist = "a:d:f:hj:m:s:";

int algorithm = 0;
int maxsim = MAX_SIM_TIME;
int start_delay = SIM_START_DELAY;
int jitter = SIM_DEFAULT_JITTER;
int last_changed_ev = 0;
int shared_links = 0;
int num_nodes = 0;
node_t *nodes = NULL;

int 
main(int argc, char *argv[])
{
	json_t			*js_obj;
	json_error_t	js_error;
	char			*rec;
    char            *cmd = *argv;
    event_t         *ev = NULL;
    int             opt;

    while ((opt = getopt(argc, argv, optlist)) != EOF) {
        switch (opt) {
            case 'a':
                algorithm = atoi(optarg);
                break;
            case 'd':
                start_delay = atoi(optarg);
                break;
            case 'f':
                printf("Filename = %s\n", optarg);
                js_obj = json_load_file(optarg, 0, &js_error);
                if (!js_obj) {
                    printf("error loading %s: %s\n", argv[1], js_error.text);
                    usage(cmd);
                }
                break;
            case 'h':
                usage(cmd);    // this will exit
                break;
            case 'j':
                jitter = atoi(optarg);
                break;
            case 'm':
                maxsim = atoi(optarg);
                break;
            case 's':
                shared_links = atoi(optarg);
                break;
            default:
                printf("Invalid option '%c', terminating\n", opt);
                usage(cmd);    // this will exit
                break;
        }
    }

#ifdef DEBUG_OUTPUT
    printf("Running: %s\n", cmd);
    printf("  algorithm = %d\n", algorithm);
    printf("  maxsim = %d\n", maxsim);
    printf("  start delay = %d\n", start_delay);
    printf("  shared links = %d\n", shared_links);
    printf("\n");
#endif


/*
	rec = json_dumps(js_obj, (JSON_ENSURE_ASCII | JSON_INDENT(4)));
	printf("%s\n", rec);
	free(rec);
*/

	if (get_nodes(js_obj, &nodes) < 0) {
		printf("error getting nodes\n");
		return 0;
	}

	if (make_connections(js_obj, nodes) < 0) {
		printf("error making connections\n");
		return 0;
	}

	json_decref(js_obj);

    if (add_shared_links() < 0) {
		printf("error adding shared links\n");
		return 0;
	}

    printf("Before run\n");
	dump_nodes(nodes);

    ev_init();
//    test_ev();
//    ev_clean();

    start_nodes(nodes);
//    ev_dump();

    while (ev = ev_get()) {
        node_t *n = ev->ev_owner;
        switch (ev->ev_type) {
            case EV_LLDP_TX:
                do_lldp_tx(ev);
                break;
            case EV_LINK_FAIL:
                printf("%d: Link Fail\n", ev_timenow());
                break;
            case EV_SYS_FAIL:
                printf("%d: System Fail\n", ev_timenow());
                break;
            case EV_SYS_START:
                system_start(n);
                break;
            case EV_LLDP_RX:
                do_lldp_rx(ev);
                break;
            default:
                printf("%d: Invalid Event\n", ev_timenow());
                break;
        }
        free(ev);

        if (ev_timenow() > maxsim) {
            printf("\nAfter Run\n");
            printf("\nLast Something Changed Event: %d\n", last_changed_ev);
            dump_nodes(nodes);
            exit(1);
        }
    }

	return 0;
}

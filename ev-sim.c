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
#include <string.h>
#include <time.h>
#include "ev-sim.h"
#include "topo.h"

event_t *ev_head = NULL;
event_t *ev_tail = NULL;
int ev_time = 0;

void
ev_init()
{
    ev_head = NULL;
    ev_tail = NULL;

    ev_time = 0;
}

int
ev_timenow()
{
    return ev_time;
}

event_t *
alloc_ev()
{
    event_t *ev;
    
    ev = calloc(1, sizeof(event_t));
    if (ev == NULL) {
        printf("Memory fail\n");
        exit(1);    
    }
    return(ev);
}

event_t *
ev_get()
{
    event_t *ev = NULL;

    ev = ev_head;
    if (ev) {
        ev_head = ev_head->next;
        ev_time = ev->ev_time;
        ev_head->prev = NULL;
    }
    return ev;
}

void
ev_insert(event_t *ev)
{
    event_t *ev_n = ev_head;
    event_t *ev_p = NULL;

    if (ev == NULL)
        return;

/*
    node_t *n = ev->ev_owner;
    printf("ev: t=%d, e=%d, n=%d\n", ev->ev_time, ev->ev_type, n->id);
*/

    // Most the time events are added to the end

    if ((ev_tail) && (ev_tail->ev_time <= ev->ev_time)) {
        ev->prev = ev_tail;
        ev->next = NULL;
        ev_tail->next = ev;
        ev_tail = ev;
        return;
    }

    // Otherwise walk the list and insert after other equal time events

    while ((ev_n) && (ev_n->ev_time <= ev->ev_time)) {
        ev_p = ev_n;
        ev_n = ev_n->next;
    }

    if (ev_n) {
        ev->next = ev_n;
        ev->prev = ev_n->prev;
        if (ev_n->prev) 
            ev_n->prev->next = ev;
        else 
            ev_head = ev;
        ev_n->prev = ev;
    } else {
        ev->next = NULL;
        ev->prev = ev_p;
        if (ev_p)
            ev_p->next = ev;
        else
            ev_head = ev;
        ev_tail = ev;
    }
}

event_t *
ev_remove(event_t *ev)
{
    event_t *ev_n = ev_head;
    event_t *ev_p = NULL;

    if (ev == NULL)
        return(NULL);

    while ((ev_n) && (ev_n != ev)) {
        ev_p = ev_n;
        ev_n = ev_n->next;
    }

    if (ev_n) {
        if (ev_p)
            ev_p->next = ev_n->next;
        else {
            ev_head = ev_n->next;
            if (ev_head)
                ev_head->prev = NULL;
        }
        if (ev_n->next)
            ev_n->next->prev = ev_p;
        else {
            ev_tail = ev_p;
            if (ev_tail)
                ev_tail->next = NULL;
        }
        ev->next = NULL;
        ev->prev = NULL;
        return(ev);
    }
    return(NULL);
}

/**  Debug and Test **/

void
ev_dump()
{
    event_t *ev = ev_head;
    node_t  *n;
    port_t  *p;

    if (ev_head) 
        printf("0x%p, ", ev_head);
    else
        printf("NULL, ");
    if (ev_tail) 
        printf("0x%p\n", ev_tail);
    else
        printf("NULL\n");

    while(ev) {
        n = ev->ev_owner;
        p = ev->ev_port;
        if (p) 
            printf("t=%d e=%d p=0x%p nx=0x%p px=0x%p n=%d p=%d\n", ev->ev_time, ev->ev_type, 
            ev, ev->next, ev->prev, n->id, p->id); 
        else
            printf("t=%d e=%d p=0x%p nx=0x%p px=0x%p n=%d\n", ev->ev_time, ev->ev_type, 
            ev, ev->next, ev->prev, n->id); 

        ev = ev->next;
    }
}

void
test_ev()
{
    int i, j;
    event_t *ev;

    printf("\n");
    ev_dump();
    printf("\n");

    for (i=0; i<20; ++i) {

        ev = calloc(1, sizeof(event_t));
        ev->ev_type = i;
        ev->ev_time = (rand() & 0xf);

        ev_insert(ev);
        ev_dump();
        printf("\n");

    }

    for (i=0; i<20; ++i) {
        int n = rand() % (20-i);
        ev = ev_head;
        for (j=0; j<n; ++j)
            ev = ev->next;
        printf("t=%d e=%d p=0x%p\n", ev->ev_time, ev->ev_type, ev); 
        event_t *o_ev = ev_remove(ev);
        if (o_ev) free(o_ev);
        ev_dump();
        printf("\n");
    }
}

void
ev_clean()
{
    event_t *ev = ev_head;
    event_t *ev_p = NULL;

    while(ev) {
        ev_p = ev;
        ev = ev->next;
        free(ev_p);
    }

    ev_head = NULL;
    ev_tail = NULL;
}
    

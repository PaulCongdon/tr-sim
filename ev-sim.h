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

#ifndef _EV_SIM_H_
#define _EV_SIM_H_

#define EV_LLDP_TX      1
#define EV_LINK_FAIL    2
#define EV_SYS_FAIL     3
#define EV_SYS_START    4
#define EV_LLDP_RX      5

typedef struct tr_tlv {
    void    *p_peer;
    void    *n_peer;
    int     node_type;
    int     level;
    int     pattr;
} tr_tlv_t;

typedef struct event {
    struct event    *next;
    struct event    *prev;
    int             ev_type;
    int             ev_time;
    void            *ev_owner;
    void            *ev_port;
    union {
        tr_tlv_t    tr;
        void        *port;
    } u;
} event_t;

extern void ev_init();
extern event_t *ev_get();
extern void ev_insert(event_t *ev);
extern event_t *ev_remove(event_t *ev);
extern int ev_timenow();
extern event_t *alloc_ev();
extern void ev_dump();
extern void ev_clean();
extern void test_ev();

#endif

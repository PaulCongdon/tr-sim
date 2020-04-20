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
#include "topo.h"
#include "ev-sim.h"
#include "lldp-sim.h"

void
trUpdate_1(node_t *n, port_t *p, tr_tlv_t *tr)
{
    int changed = 0;
    int invalidate_others = 0;

    if (n->node_type == N_SERVER)
        return;
    else {
        if (tr->node_type == N_SERVER)
            if (n->level != 1) {
                n->level = 1;
                p->pattr = P_DOWNLINK;
                changed = 1;
                invalidate_others = 1;
            } else {
                if (p->pattr == P_DOWNLINK)
                    return;
                p->pattr = P_DOWNLINK;
                changed = 1;
            }
        else {
            if (tr->level == P_UNKNOWN)
                return;
            if (tr->level == n->level)
                if (p->pattr == P_CROSSLINK)
                    return;
                else {
                    p->pattr = P_CROSSLINK;
                    changed = 1;
                }
            if (tr->level == n->level+1)
                if (p->pattr == P_UPLINK)
                    return;
                else {
                    p->pattr = P_UPLINK;
                    changed = 1;
                }
            if (tr->level == n->level-1)
                if (p->pattr == P_DOWNLINK)
                    return;
                else {
                    p->pattr = P_DOWNLINK;
                    changed = 1;
                }
            if ((tr->level < n->level-1) || (n->level == P_UNKNOWN)) {
                n->level = tr->level+1;
                p->pattr = P_DOWNLINK;
                changed = 1;
                invalidate_others = 1;
            }
        }
    }

    if (changed) 
        somethingChangedLocal(n, p);
    
    if (invalidate_others) {
        port_t *other_p;
        int i;
        for (i=0; i<n->port_ct; ++i) {
            other_p = &n->ports[i];
            if ((other_p != p) && (other_p->pattr != P_UNKNOWN)) {
                other_p->pattr = P_UNKNOWN;
                somethingChangedLocal(n, other_p);
            }
        }
    }
}

void
trUpdate_2(node_t *n, port_t *p, tr_tlv_t *tr)
{
    int changed = 0;
    int invalidate_others = 0;

    if (n->node_type == N_SERVER)
        return;
    else {
        if (tr->level == P_UNKNOWN)
            return;
        if (tr->level == n->level)
            if (p->pattr == P_CROSSLINK)
                return;
            else {
                p->pattr = P_CROSSLINK;
                changed = 1;
            }
        if (tr->level == n->level+1)
            if (p->pattr == P_UPLINK)
                return;
            else {
                p->pattr = P_UPLINK;
                changed = 1;
            }
        if (tr->level == n->level-1)
            if (p->pattr == P_DOWNLINK)
                return;
            else {
                p->pattr = P_DOWNLINK;
                changed = 1;
            }
        if ((tr->level < n->level-1) || (n->level == P_UNKNOWN)) {
            n->level = tr->level+1;
            p->pattr = P_DOWNLINK;
            changed = 1;
            invalidate_others = 1;
        }
    }

    if (changed) 
        somethingChangedLocal(n, p);
    
    if (invalidate_others) {
        port_t *other_p;
        int i;
        for (i=0; i<n->port_ct; ++i) {
            other_p = &n->ports[i];
            if ((other_p != p) && (other_p->pattr != P_UNKNOWN)) {
                other_p->pattr = P_UNKNOWN;
                somethingChangedLocal(n, other_p);
            }
        }
    }
}

void
trUpdate(node_t *n, port_t *p, tr_tlv_t *tr)
{
    switch(algorithm) {
        case 1: trUpdate_1(n,p,tr);
                break;
        case 2: trUpdate_2(n,p,tr);
                break;
        default:
                printf("Invalid algorithm %d\n", algorithm);
                break;
    }
}

void
somethingChangedRemote(node_t *n, port_t *p, tr_tlv_t *tr)
{
#ifdef DEBUG_OUTPUT
    printf("somethingChangedRemote(): n=%d, p=%d ", n->id, p->id);

    printf("t: %d=>%d ", p->last_peer_node_type, tr->node_type); 
    printf("l: %d=>%d ", p->last_peer_level, tr->level);
    printf("a: %d=>%d\n", p->last_peer_pattr, tr->pattr);
#endif

    p->last_peer_node_type = tr->node_type;
    p->last_peer_level = tr->level;
    p->last_peer_pattr = tr->pattr;

    trUpdate(n, p, tr);
}

void
somethingChangedLocal(node_t *n, port_t *p)
{
    int     credit;
    int     delay;
    event_t *o_ev = NULL;

    // Debug and save old values

#ifdef DEBUG_OUTPUT
    printf("somethingChangedLocal(): n=%d, p=%d ", n->id, p->id);

    printf("t: %d=>%d ", n->my_last_node_type, n->node_type); 
    printf("l: %d=>%d ", n->my_last_level, n->level);
    printf("a: %d=>%d\n", p->my_last_pattr, p->pattr);
#endif

    n->my_last_node_type = n->node_type;
    n->my_last_level = n->level;
    p->my_last_pattr = p->pattr;

    // Determine if we can send

    credit = p->tx_credit + (ev_timenow() - p->last_tx_time);
    if (credit > LLDP_MAX_TX_CREDIT)
        credit = LLDP_MAX_TX_CREDIT;
    p->tx_credit = credit;

    if (p->tx_credit > 0)
        delay = 1;
    else {
        if (p->tx_fast)
            delay = 1;
        else
            delay = LLDP_MSG_INTERVAL;
    }

    // Update the event and re-use

    if (p->next_ev)
        o_ev = ev_remove(p->next_ev);

    // Schedule the next transmit time

    lldp_xmit(n, p, delay, o_ev);
}

void
lldp_init()
{
}

void
lldp_xmit(node_t *n, port_t *p, int delay, event_t *o_ev)
{
    event_t *ev;

    //printf("xmit: n=%d, p=%d, delay=%d\n", n->id, p->id, delay);

    if (o_ev)
        ev = o_ev;
    else
        ev = alloc_ev();

    ev->ev_type = EV_LLDP_TX;
    ev->ev_time = ev_timenow() + delay;
    ev->ev_owner = n;
    ev->ev_port = p;
    ev_insert(ev);

    p->next_ev = ev;

    // NOTE: we take the snapshot of the parameters when sending
}

void
do_lldp_tx(event_t *ev)
{
    event_t *ev_rx;
    port_t  *p, *p_peer;
    node_t  *n, *n_peer;
    int     delay;

    p = ev->ev_port;
    n = ev->ev_owner;
    if ((!p) || (!n)) {
        printf("Bad event pointers\n");
        exit(1);
    }

    p_peer = p->peer;
    if (p_peer)
        n_peer = p_peer->node;
    else {
        printf("Bad peer pointer\n");
        exit(1);
    }

    if ((!p->status) || (!n->status)) {
        printf("%d: unable to tx\n", ev_timenow());
        return;
    }

#ifdef DEBUG_OUTPUT
    printf("%d: TX n=%d->%d, p=%d, t=%d, l=%d, a=%d\n", ev_timenow(), 
            n->id, n_peer->id, p->id, n->node_type, n->level, p->pattr);
#endif

    ev_rx = alloc_ev();
    ev_rx->ev_type = EV_LLDP_RX;
    ev_rx->ev_time = ev_timenow();
    ev_rx->ev_owner = n_peer;
    ev_rx->ev_port = p_peer;

    ev_rx->u.tr.p_peer = p;
    ev_rx->u.tr.n_peer = n;
    ev_rx->u.tr.node_type = n->node_type;
    ev_rx->u.tr.level = n->level;
    ev_rx->u.tr.pattr = p->pattr;

    ev_insert(ev_rx);

    --p->tx_credit;
    p->last_tx_time = ev_timenow();

    if (p->tx_fast) {
        --p->tx_fast;
        delay = 1;
    } else
        delay = LLDP_MSG_INTERVAL;

    lldp_xmit(n, p, delay, NULL);
}

void
do_lldp_rx(event_t *ev)
{
    node_t  *n, *n_peer;
    port_t  *p, *p_peer;

    n = ev->ev_owner;
    p = ev->ev_port;

    n_peer = ev->u.tr.n_peer;
    p_peer = ev->u.tr.p_peer;

    if ((!n) || (!p) || (!n_peer) || (!p_peer)) {
        printf("Bogus LLDP RX event\n");
        exit(1);
    }

#ifdef DEBUG_OUTPUT
    printf("%d: RX n=%d<-%d, p=%d, t=%d, l=%d, a=%d\n", ev_timenow(), 
            n->id, n_peer->id, p->id, 
            ev->u.tr.node_type, ev->u.tr.level, ev->u.tr.pattr);
#endif

    if ((p->last_peer_node_type != ev->u.tr.node_type) ||
        (p->last_peer_level != ev->u.tr.level) ||
        (p->last_peer_pattr != ev->u.tr.pattr) ) 
            somethingChangedRemote(n, p, &ev->u.tr);
}


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
#ifndef _LLDP_SIM_H_
#define _LLDP_SIM_H_

#include "topo.h"
#include "ev-sim.h"

#define LLDP_MAX_TX_CREDIT  5
#define LLDP_MSG_INTERVAL   30
#define LLDP_MAX_FAST       4

extern void lldp_init();
extern void lldp_xmit(node_t *n, port_t *p, int delay, event_t *o_ev);

extern void do_lldp_tx(event_t *ev);
extern void do_lldp_rx(event_t *ev);

extern void somethingChangedRemote(node_t *n, port_t *p, peer_t *x, tr_tlv_t *tr);
extern void somethingChangedLocal(node_t *n, port_t *p);

#endif

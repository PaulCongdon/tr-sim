all: tr-sim

clean:
	-rm topo.o ev-sim.o lldp-sim.o tr-sim

lldp-sim.o: lldp-sim.c lldp-sim.h topo.h ev-sim.h
	cc -c -g lldp-sim.c
	
topo.o: topo.c topo.h ev-sim.h lldp-sim.h
	cc -c -g topo.c

ev-sim.o: ev-sim.c ev-sim.h topo.h
	cc -c -g ev-sim.c

tr-sim: topo.o ev-sim.o lldp-sim.o
	cc -g -o tr-sim topo.o ev-sim.o lldp-sim.o -ljansson

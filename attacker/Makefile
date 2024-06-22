all: cachewarp_blind_drop

cachewarp_blind_drop: ctrl_apic.c
	gcc ctrl_apic.c ../frameworks/libtea/libtea_irq_entry.o -I. -o ./cachewarp_blind_drop -DBLIND
clean:
	rm -f ./cachewarp_blind_drop


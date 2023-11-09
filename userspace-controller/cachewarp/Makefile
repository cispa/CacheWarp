all: bench blind ssh_exp sudo_exp 

bench: cachewarp.c
	gcc cachewarp.c ../libtea_irq_entry.o -I. -o ./bench -DBENCH
blind: cachewarp.c
	gcc cachewarp.c ../libtea_irq_entry.o -I. -o ./blind -DRSA
ssh_exp: cachewarp.c
	gcc cachewarp.c ../libtea_irq_entry.o -I. -o ./ssh_exp -DSSH
sudo_exp: cachewarp.c
	gcc cachewarp.c ../libtea_irq_entry.o -I. -o ./sudo_exp -DSUDO
clean:
	rm -f bench blind ssh_exp sudo_exp 


out: message_slot.o message_sender.o message_reader.out
	gcc message_slot.o message_sender.o message_reader.o -o out

message_slot.o: message_slot.c message_slot.h
	gcc -c message_slot.h message_slot.c  # compiled with -c to make .o object only (no linking)
message_sender.o: message_sender.c
	gcc -c message_sender.c
message_reader.o: message_reader.c
	gcc -c message_reader.c

clear:
	rm message_slot.o message_reader.o message_sender.o
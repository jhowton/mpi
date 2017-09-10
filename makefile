p2:  ppexec.cpp libpp.cpp t1.c t2.c t3.c t4.c t5.c t6.c p3_tester.c p4_tester.c P5TEST1.c P5TEST2.c P5TEST3.c
	g++ -std=c++11 -o ppexec ppexec.cpp
	g++ -std=c++11 -c libpp.cpp -o libpp.a
	gcc -o t1 t1.c libpp.a -lstdc++
	gcc -o t2 t2.c libpp.a -lstdc++
	gcc -o t3 t3.c libpp.a -lstdc++
	gcc -o t4 t4.c libpp.a -lstdc++
	gcc -o t5 t5.c libpp.a -lstdc++
	gcc -o t6 t6.c libpp.a -lstdc++
	gcc -o p3_tester p3_tester.c libpp.a -lstdc++
	gcc -o p4_tester p4_tester.c libpp.a -lstdc++
	gcc -o P5TEST1 P5TEST1.c libpp.a -lstdc++
	gcc -o P5TEST2 P5TEST2.c libpp.a -lstdc++
	gcc -o P5TEST3 P5TEST3.c libpp.a -lstdc++
debug:  ppexec.cpp libpp.cpp t1.c t2.c t3.c t4.c t5.c t6.c p3_tester.c p4_tester.c P5TEST1.c P5TEST2.c P5TEST3.c
	g++ -std=c++11 -o ppexec ppexec.cpp
	g++ -std=c++11 -c libpp.cpp -o libpp.a
	gcc -g -o t1 t1.c libpp.a -lstdc++
	gcc -g -o t2 t2.c libpp.a -lstdc++
	gcc -g -o t3 t3.c libpp.a -lstdc++
	gcc -g -o t4 t4.c libpp.a -lstdc++
	gcc -g -o t5 t5.c libpp.a -lstdc++
	gcc -g -o t6 t6.c libpp.a -lstdc++
	gcc -g -o p3_tester p3_tester.c libpp.a -lstdc++
	gcc -g -o p4_tester p4_tester.c libpp.a -lstdc++
	gcc -g -o P5TEST1 P5TEST1.c libpp.a -lstdc++
	gcc -g -o P5TEST2 P5TEST2.c libpp.a -lstdc++
	gcc -g -o P5TEST3 P5TEST3.c libpp.a -lstdc++
clean: 
	rm -f libpp.a
	rm -f *.o
	rm -f ppexec
	rm -f p3_tester
	rm -f p4_tester
	rm -f P5TEST?
	rm -f t?

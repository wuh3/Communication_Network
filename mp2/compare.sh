rm ./test
make 
./reliable_receiver 1235 ./test &  PIDMIX=$!
./reliable_sender 127.0.0.1 1235 ./Makefile 1000 &  PIDIOS=$!
wait
diff ./Makefile test

rm ./output.txt
make 
./distvec topofile messagefile changesfile ./output.txt &  PIDMIX=$!
wait
diff ./output.txt model.txt

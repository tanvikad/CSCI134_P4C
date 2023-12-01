make clean
echo "LOG test1" > input.txt
echo "STOP" >> input.txt
echo "START" >> input.txt
echo "SCALE=C" >> input.txt
echo "SCALE=F" >> input.txt
echo "PERIOD=2" >> input.txt
echo "OFF" >> input.txt
make
./lab4b --log log.txt < input.txt

for word in "LOG" "STOP" "START" "SCALE=C" "SCALE=F" "PERIOD=2" "OFF"
do
    if ! (grep $word log.txt); then
        echo "not found"
    fi
done

if egrep "[0-9]*:[0-9]*:[0-9]* SHUTDOWN" log.txt; then
    echo ""
else 
    echo "not found"
fi
test -f ./dzsh &&
for x in TEST/case.*
do
	echo "----------  $x --------------------------------------"
	cat $x
	echo "----------  test result -----------------------------"
	./dzsh $x
	read
	clear
done

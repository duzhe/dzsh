make ver=debug
for x in TEST/case.*
do
	echo "----------  $x --------------------------------------"
	cat $x
	echo "----------  test result -----------------------------"
	./dzsh $x
	read
	clear
done

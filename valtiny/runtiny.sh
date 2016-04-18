rm tiny
cc -I . -g -o tiny -DRUN fake.c -lpthread
if ./tiny
then
	echo successful
else
	echo FAILED
fi

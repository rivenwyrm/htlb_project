clean:
	rm -f ./test_app
test_app:
	gcc -Wall htlb_app.c -o test_app
all: test_app

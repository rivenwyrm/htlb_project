test_app: htlb_app.c
	gcc -Wall htlb_app.c -o test_app
all: test_app

clean:
	rm -f ./test_app

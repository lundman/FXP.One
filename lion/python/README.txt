
Welcome to the LiON python support project.



You will need SWIG, preferable something recent. The Python
development install, ie, with Python include files.

In here we have prepared the SWIG file "lion.i" so there should be
just a matter of running "make" and test against your Python code.

We have provided a Python example "test.py" which should connect to
www.paypal.com with SSL and fetch index.




Manual steps if you wish not to use the Makefile are:


# Make lion
% make -C .. -D_lion_userinput=lion_userinput

# From lion.i we generate lion.py and lion_wrap.c
% swig -python -DWITH_SSL lion.i

# We compile lion_wrap.c to lion_wrap.o
% gcc -c lion_wrap.c -I ../ -I /usr/local/include/python2.3/ -DWITH_SSL 

# We link lion_wrap.so and lion.a to make .so
% gcc -lcrypto -lssl -shared lion_wrap.o ../*.o  -o _lion.so





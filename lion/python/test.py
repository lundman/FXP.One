#!/usr/local/bin/python
#
import lion
import sys
import time

DONE = False 
def output(x,y,z,a,b):
   global DONE
   if (z == lion.LION_CONNECTION_CLOSED):
      DONE = True
   if (z == lion.LION_INPUT):
      print b
   if (z == lion.LION_CONNECTION_CONNECTED):
      lion.lion_printf(x,"GET / HTTP/1.1\nHost: www.paypal.com\n\n")
      
lion.lion_init()
conn = lion.lion_connect("www.paypal.com",443,0,0,lion.LION_FLAG_FULFILL,None)

lion.lion_set_handler(conn,output)

lion.lion_ssl_set(conn,lion.LION_SSL_CLIENT)


while (not DONE):
   lion.lion_poll(0,1)




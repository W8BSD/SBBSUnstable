// $Id$

load("sockdefs.js");

var socket;
if(this.client)
	socket=client.socket;
else
	socket=new Socket(SOCK_STREAM,"test");

var option_list;
if(socket.option_list)
	option_list = socket.option_list; // dynamic list (v3.13b)
else
 	option_list = sockopts; // static list (sockdefs.js)
var opt;
for(opt in option_list)
	print(option_list[opt] +" = "+ socket.getoption(option_list[opt]));
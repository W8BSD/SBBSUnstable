// chat_sec.js

// Chat Section for any/all Synchronet command shells

// $Id$

load("sbbsdefs.js");
load("nodedefs.js");

// Over-ride these default values by creating/modifying the [chat_sec] section in your ctrl/modopts.ini file
var irc_server = "irc.synchro.net 6667";
var irc_channel = "#Synchronet"
var options = load("modopts.js", "chat_sec");
if (options) {
    if (options.irc_server)
        irc_server = options.irc_server;
    if (options.irc_channel)
        irc_channel = options.irc_channel;
}
var cmdkey;

if(user.compare_ars("rest C")) {
    write("\r\nYou can't chat.\r\n");
	exit(0);
}

// Set continue point for main menu commands
menu:
while(1) {
	var str="";

	// Display TEXT\MENU\CHAT.* if not in expert mode
	if(!(user.settings & USER_EXPERT)) {
		bbs.menu("chat");
	}

	// Update node status
	bbs.node_action=NODE_CHAT;

	// async
	write("\r\n\001_\1y\001hChat: \001n");

	switch(cmdkey=console.getkeys("ACDFIJPQRST?\r",K_UPPER)) {
		case "S":
			user.chat_settings ^= CHAT_SPLITP;
			write("\001n\r\nPrivate split-screen chat is now: \001h");
			write((user.chat_settings & CHAT_SPLITP)?"ON\001n":"OFF\001n");
			writeln("");
			break;
		case "A":
			writeln("");
			user.chat_settings ^= CHAT_NOACT;
			system.node_list[bbs.node_num-1].misc ^= NODE_AOFF;
			bbs.whos_online();
			break;
		case 'D':
			writeln("");
			user.chat_settings ^= CHAT_NOPAGE;
			system.node_list[bbs.node_num-1].misc ^= NODE_POFF;
			bbs.whos_online();
			break;
		case 'F':
			writeln("");
			bbs.exec("?finger");
			break;
		case 'I':
			writeln("");
			bbs.exec("?sbbsimsg");
			break;
		case 'R':
			{
				if(user.security.level >= 90 || user.security.exemptions&UFLAG_C) {
					write("\r\n\001n\001y\001hIRC Server: ");
					server=console.getstr(irc_server, 40, K_EDIT|K_LINE|K_AUTODEL);
					if(console.aborted)
						break;
				}
				write("\r\n\001n\001y\001hIRC Channel: ");
				var channel=console.getstr(irc_channel, 40, K_EDIT|K_LINE|K_AUTODEL);
				if(!console.aborted)
					bbs.exec("?irc -a " + irc_server + " " + channel);
			}
			break;
		case 'J':
			bbs.multinode_chat();
			break;
		case 'P':
			bbs.private_chat();
			break;
		case 'C':
			if(!bbs.page_sysop())
				bbs.page_guru();
			break;
		case 'T':
			bbs.page_guru();
			break;
		case '?':
			if(user.settings & USER_EXPERT)
				bbs.menu("chat");
			break;
		default:
			break menu;
	}
}

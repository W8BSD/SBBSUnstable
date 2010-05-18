/* 	IRC Bot Module - Server Commands
	You would place all of your module functions in this file.	*/
	
function Server_command(srv,cmdline,onick,ouh) 
{
	var cmd=IRC_parsecommand(cmdline);
	switch (cmd[0]) {
		case "JOIN":
			if (onick == srv.curnick) break;
			
			// Someone else joining? Let's send them a private welcome message!
			srv.o(onick,"Welcome to Poker!");
			srv.o(onick,"This is a module for IRCBot - by Cyan");
			break;
		case "PRIVMSG":
			if ((cmd[1][0] == "#") || (cmd[1][0] == "&")) {
				var chan = srv.channel[cmd[1].toUpperCase()];
				if (!chan)
					break;
				if (!chan.is_joined)
					break;
				if(srv.users[onick.toUpperCase()]) {
					/* You can do special command processing here, if you like.
					   This is currently set up to parse public room messages
					   for things like trivia answers, or other responses that
					   are inconvenient for users to submit with a command
					   prefix */
				}
			}
			break;
		default:
			break;
	}
}

//////////////////// Non-object Functions ////////////////////
function poker_deal_hole_cards(target,srv) {
	var poker_game=poker_games[target];
	for (p in poker_game.users) {
		poker_game.users[p].cards[0] = poker_game.deck.deal();
		poker_game.users[p].cards[1] = poker_game.deck.deal();
		srv.o(p,"Your hole cards: " 
			+ poker_game.users[p].cards[0].color + "[ "
			+ poker_game.users[p].cards[0].char + " ] "
			+ poker_game.users[p].cards[1].color + "[ "
			+ poker_game.users[p].cards[1].char + " ]"
		,"NOTICE");
	}
}

function poker_next_turn(target,srv) {
	var poker=poker_games[target];
	poker.turn++;
	if(poker.turn==poker.users_map.length) poker.turn=0;
	
	
	if(poker.deal_next) {
		poker_load_pot(target,srv);
		switch(++poker.round) {
			case 1:
				poker_deal_flop(target,srv);
				break;
			case 2:
				poker_deal_turn(target,srv);
				break;
			case 3:
				poker_deal_river(target,srv);
				break;
			default:
				poker_compare_hands(target,srv);
				break;
		}
		poker.deal_next=false;
	} else {
		var turn_user=poker.users[poker.users_map[poker.turn]];
		if(poker.current_bet==turn_user.bet) poker.deal_next=true;
	}
	if(poker.round<4) poker_prompt_player(target,srv);
}

function poker_compare_hands(target,srv) {
	var poker=poker_games[target];
	var winning_hand=-1;
	var winning_player=-1;
	for(var p in poker.users) {
		var player=poker.users[p];
		var hand=poker.community_cards.concat(player.cards)
		var rank=Rank(hand);
		if(rank>winning_hand) {
			winning_hand=rank;
			winning_player=p;
		}
	}
	srv.o(target,winning_player + " won this hand with " + RANKS[winning_hand]);

}

function poker_deal_flop(target,srv) { 
	var poker_game=poker_games[target];
	poker_game.community_cards[0] = poker_game.deck.deal();
	poker_game.community_cards[1] = poker_game.deck.deal();
	poker_game.community_cards[2] = poker_game.deck.deal();
	srv.o(target, "The Flop: "
		+ poker_show_card(poker_game.community_cards[0])
		+ poker_show_card(poker_game.community_cards[1])
		+ poker_show_card(poker_game.community_cards[2])
	);
}

function poker_deal_turn(target,srv) {
	var poker_game=poker_games[target];
	poker_game.community_cards[3] = poker_game.deck.deal();
	srv.o(target, "The Turn: "
		+ poker_show_card(poker_game.community_cards[0])
		+ poker_show_card(poker_game.community_cards[1])
		+ poker_show_card(poker_game.community_cards[2])
		+ poker_show_card(poker_game.community_cards[3])
	);
}

function poker_deal_river(target,srv) {
	var poker_game=poker_games[target];
	poker_game.community_cards[4] = poker_game.deck.deal();
	srv.o(target, "The River: " 
		+ poker_show_card(poker_game.community_cards[0])
		+ poker_show_card(poker_game.community_cards[1])
		+ poker_show_card(poker_game.community_cards[2])
		+ poker_show_card(poker_game.community_cards[3])
		+ poker_show_card(poker_game.community_cards[4])
	);
}

function poker_show_card(card) {
	return(card.color + "[ " + card.char + " ] ");
}

function poker_load_pot(target,srv) {
	var poker=poker_games[target];
	for(var p in poker.users) {
		poker.pot+=poker.users[p].bet;
		poker.users[p].bet=0;
	}
	srv.o(target,"Current pot: $" + poker.pot);
	return;
}

function poker_prompt_player(target,srv) {
	var poker=poker_games[target];
	var turn=poker.users_map[poker.turn];
	srv.o(turn,"It is your turn. Minimum bet: $" + poker.current_bet,"NOTICE");
}

function poker_verify_game_status(target,srv,onick) {
	var poker=poker_games[target];
	if (!poker) {
		srv.o(target, "No poker game in progress. Type '" + get_cmd_prefix()
			+ "DEAL' to start a new one.")
		return false;
	} 
	if(poker.round<0) {
		srv.o(target, onick + ", the game hasn't started yet.");
		return false;
	}
	if(!poker.users[onick.toUpperCase()] || !poker.users[onick.toUpperCase()].active) {
		srv.o(onick, "You're not even in the hand!");
		return false;
	}
	var turn_player=poker.users_map[poker.turn];
	if (turn_player != onick.toUpperCase()) {
		srv.o(target, "Acting out of turn?");
		return false;
	}
	return true;
}

function poker_init_hand(target) {
	poker_games[target].deck.shuffle();
	for(var u in poker_games[target].users) {
		poker_games[target].users_map.push(u);
	}
}

function load_scores()
{
	var s_file=new File(poker_dir + "scores.ini");
	if(s_file.open(file_exists(s_file.name)?"r+":"w+")) {
		writeln("reading scores from file: " + s_file.name);
		var players=s_file.iniGetKeys();
		for(var p in players) {
			writeln("loading player score: " + players[p]);
			poker_scores[players[p]]=s_file.iniGetValue(null,players[p]);
		}
		s_file.close();
	}
}


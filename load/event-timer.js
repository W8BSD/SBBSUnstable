/*     
	Event Timer  - for Synchronet 3.15a+ (2011)
	
	-	code by mcmlxxix
	
	methods:
	
	-	Timer.cycle()
	-	Timer.addEvent(interval,repeat,action,arguments,context)
		
		- interval = time to wait before running event (in milliseconds)
		- repeat = how many times to run the event (TRUE = run infinitely, FALSE = run once)
		- action = the function to call when running the event
		- arguments = the arguments to pass to the action 
		- context = the context (scope) in which to run the event
	
	sample usage: 
	
		load(event-timer.js);
		var timer=new Timer();
		
		function event() {
			print("hello\r\n");
		}
		timer.addEvent(10,10,event);
		
		//will print "hello" 10 times at 10 second intervals
		
		while(timer.events.length > 0) {
			timer.cycle();
			mswait(1000);
		}
		
		//script will end when the event runs its course
*/
function Timer() {
	this.VERSION = "$Revision$".replace(/\$/g,'').split(' ')[1];
	this.events = [];
	
	/* called by parent script, generally in a loop, and generally with a pause or timeout to minimize cpu usage */
	this.cycle = function() {
		var now = Date.now();
		var count = 0;
		/* scan all events and run any that are due */
		for(var e = 0; e<this.events.length; e++) {
			var event = this.events[e];
			if(now - event.lastrun >= event.interval) {
				/* run event */
				event.run();
				count++;
				/* an event with a repeat set to TRUE will never expire */
				if(event.repeat === true)
					continue;
				/* decrement event repeat counter */
				if(event.repeat > 0)
					event.repeat--;
				/* if event has expired, or is set to run only once, delete it */
				if(!event.repeat) 
					this.events.splice(e--,1);
			}
		}
		/* return the number of events run this cycle */
		return count;
	}
	
	/* create a new event, do not include () on action parameter */
	this.addEvent = function(interval,repeat,action,arguments,context) {
		var event=new Event(interval,repeat,action,arguments,context);
		this.events.push(event);
	}
	
	/* event object created by addEvent */
	function Event(interval,repeat,action,arguments,context) {
		/* last time_t at which event was executed */
		this.lastrun = Date.now();
		/* seconds between event occurance */
		this.interval=interval;
		/* number of times to repeat, true to repeat indefinitely, false to run only once */
		this.repeat=repeat;
		/* function called when event is run */
		this.action=action;
		/* arguments passed to function */
		this.arguments=arguments;
		/* context in which to run function */
		this.context=context;
		/* run event */
		this.run = function() {
			this.action.apply(this.context,this.arguments);
			this.lastrun = Date.now();
		}
	}
};



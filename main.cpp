#include <algorithm>
#include <climits>
#include <fstream>
#include <iostream>
#include <stdlib.h>

#include "cl_parser.h"
#include "PCB.h"
#include "proc_queues.h"

/*************************************************
 ******************** TYPES **********************
 ************************************************/
/* used by parse_input and contains the command-line options. */
extern std::string help_output;		

/* used by promote_priority */
enum ops_t 
{
	IO,
	AGE
};

/* enum to help identify which scheduler to user */
enum scheduler_t 
{
	FIFO,
	SJF,
	PRIORITY,
	EDF
};

/* user provided input values */
struct env_t 
{
	int kernel_tq;
	int user_tq;
	int age_time;
	int age_val;
	bool interactive;
	scheduler_t scheduler;
	std::string file_name;
	std::ofstream outfs;

	void print()
	{
		std::cerr << "kernel_tq: " << kernel_tq << std::endl;
		std::cerr << "user_tq: " << user_tq << std::endl;
		std::cerr << "age_time: " << age_time << std::endl;
		std::cerr << "age_value: " << age_val << std::endl;
		std::cerr << "scheduler: " << scheduler << std::endl;
		std::cerr << "file-name: " << file_name << std::endl;
		std::cerr << std::endl; 
 	}
};

/* object useful for computing average waiting and turnaround time */
struct stats_t
{
	double awt, att;
	size_t np;

	void print()
	{
		printf("NP: %zu\n", np);
		printf("AWT: %.3f\n", awt);
		printf("ATT: %.3f\n", att);
	}
};

/* Ready queue with a corresponding age queue. The age queue is
 * synchronized with the ready queue but is ordered by age time,
 * where older processes would be popped before younger ones.
 * Requires a little over twice the memory requirement compared
 * to just the age_q. The advantage is that ageing per iteration
 * is done in log(n) time instead of linear time. 
 */
template<typename T>
struct ready_age_t 
{
	T ready_q;
	age_t<typename T::iterator> age_q;

	void push(PCB& x)
	{
		ready_q.push(x);
		age_q.push(ready_q.search(x));
		assert(age_q.size() == ready_q.size());
	}

	void pop()
	{
		typename T::iterator iter;
		iter = ready_q.search(ready_q.top());
		age_q.erase(age_q.search(iter));
		ready_q.pop();

		assert(age_q.size() == ready_q.size());
	}

	PCB top()
	{
		return ready_q.top();
	}

	bool empty()
	{
		return ready_q.empty();
	}

	size_t size()
	{
		return ready_q.size();
	}
};

/**************************************************
 ********************* MACROS *********************
 *************************************************/

#define ISMAXED(x) (x==49 || x==99)
#define CLOCK_LAST (size_t) INT_MIN
#define ISKERNEL(x) (x>=50 && x<=99)
#define DEFAULT_AGE_TIME 100
#define DEFAULT_AGE_VALUE 10
#define DEFAULT_KERNEL_QUANTUM 100
#define DEFAULT_USER_QUANTUM 25
#define DEFAULT_SCHEDULER PRIORITY

#define GETS_CPU "Gets CPU"
#define END "End"
#define TQ_INTER "Clock Interrupt"
#define IO_INTER "I/O Interrupt"
#define AGED "Has Aged"
#define ABORT "Cannot Meet Deadline"

#define PRINT_STATE(os, Clock, x, state) (os << Clock << "\t" << (x).pid << "\t" << state << std::endl)
#define PRINT_STATE_INTER(os, Clock, x, state) (os << "process " << (x).pid << " \'" << state << "\'" << std::endl)
#define INTERACTIVE_WAIT(inp, line) while (std::getline(inp, line) && !line.empty())

/**************************************************
 ************* FUNCTOIN PROTOTYPES **************
 **************************************************/

void parse_input(int, char**, env_t&);

template<typename T>
void update(new_t&, ready_age_t<T>&, size_t Clock);

template<typename T>
void do_io(ready_age_t<T>&, io_t&, env_t&, size_t);

template<typename T>
void do_aging(ready_age_t<T>&, env_t&, size_t);

template<typename T>
void print_states(new_t&, ready_age_t<T>&, io_t&, PCB, bool, int, env_t&);

template<typename T>
void run_scheduler(new_t&, ready_age_t<T>&, env_t&, stats_t&);

inline void demote_priority(PCB&, env_t&);

inline void promote_priority(PCB&, ops_t, env_t&);


/*****************************************************
 *************** FUNCTION DEFINITIONS ****************
 ****************************************************/
int main(int argc, char *argv[])
{
	/* get the user-defined values */
	env_t env;
	stats_t stats;
	parse_input(argc, argv, env);

	/* read the input file and create the new queue */
	new_t new_q = create_new_queue(env.file_name);
	stats.np = new_q.size();
	stats.awt = 0.0; stats.att=0.0;

	/* run the appropriate scheduler */
	if (env.scheduler == FIFO)
	{
		ready_age_t<fifo_t> ready_age_q;
		run_scheduler(new_q, ready_age_q, env, stats);		
	}
	else if (env.scheduler == PRIORITY)
	{
		ready_age_t<priority_t> ready_age_q;
		run_scheduler(new_q, ready_age_q, env, stats);	
	}
	else if (env.scheduler == SJF)
	{
		ready_age_t<sjf_t> ready_age_q;
		run_scheduler(new_q, ready_age_q, env, stats);	
	}
	else if (env.scheduler == EDF)
	{
		ready_age_t<edf_t> ready_age_q;
		run_scheduler(new_q, ready_age_q, env, stats);
	}
	
	/* print the stats */
	stats.att /= (double) stats.np;
	stats.awt /= (double) stats.np; 
	std::cout << "************* STATS *************" << std::endl;
	stats.print();

	/* all done folks */
	env.outfs.close();
	return 0;
}


/*************************************************************************/
/*! This functions runs the scheduler given by the user
    \param new_q is the queue of processes organized by arrival time
    \param ready_age_q is the queue of processes organized by scheduler
    			 with a corresponding age_q
    \param env is the struct of user-provided values
*/
/*************************************************************************/    
template<typename T>
void run_scheduler(new_t& new_q, ready_age_t<T>& ready_age_q, env_t &env, stats_t &stats)
{
	io_t io_q;
	bool running=false; // is there a process running?
	PCB x;
	int tq, org_tq;
	std::string line;
	std::vector<size_t> wait; // used only by edf scheduler

	/* The first line of the output file are the column titles */
	env.outfs << "CLOCK\tPID\tACTION" << std::endl; 

	/* check if any processes are left to run */
	if (new_q.empty())
	{
		std::cerr << "No processes to run" << std::endl;
		return;
	}

	/* if edf fill the vector with 0s */
	if (env.scheduler == EDF)
		for (size_t i=0; i!=new_q.size()+1; i++)
			wait.push_back(0);

	if (env.interactive)
	{
		std::cerr << std::endl << "**** INTERACTIVE MODE ****" << std::endl;
		std::cerr << "To enter next clock cycle, press <enter>" << std::endl;
		INTERACTIVE_WAIT(std::cin, line);
	}

	/* each iteration is a clock tick */
	for (size_t Clock=0; running || !(ready_age_q.empty() && io_q.empty() && new_q.empty()); Clock++)
	{
#ifdef _DEBUG
		/* let us know which iteration is running */
		if (!env.interactive && Clock % 100 == 0)
			std::cerr << "*** Now at clock " << Clock << " ***" << std::endl;
#endif

		if (env.interactive)
		{
			std::cerr << "*** Now at clock " << Clock << " u" << env.user_tq;
			std::cerr << " k" << env.kernel_tq << " a" << env.age_time << " ***";
			std::cerr << std::endl;
		}

		/* do io to all processes in the io_q */
		do_io(ready_age_q, io_q, env, Clock);

		/* do aging */
		if (env.scheduler == PRIORITY)
			do_aging(ready_age_q, env, Clock);

		/* update the ready queue with any new arrivals */
		if (!new_q.empty())
			update(new_q, ready_age_q, Clock);

		/* if a process is running */
		if (running)
		{
			x.bst--;
			tq++;

			/* process finished running */
			if (x.bst == 0)
			{
				/* print to file termination action */
				PRINT_STATE(env.outfs, Clock, x, END);
				if (env.interactive)
					PRINT_STATE_INTER(std::cout, Clock, x, END);
				running = false;

				/* update stats */
				stats.att += (double)Clock;
			}

			/* clock-interrupted */
			else if (tq == org_tq)
			{
				/* print to file clock-interrupt action */
				PRINT_STATE(env.outfs, Clock, x, TQ_INTER);
				if (env.interactive)
					PRINT_STATE_INTER(std::cout, Clock, x, TQ_INTER);

				demote_priority(x, env);
				x.Clock = Clock; // update the time the process is pushed
				ready_age_q.push(x);
				running = false;				
			}

			/* io event occurs to an io process */
			else if (env.scheduler != EDF && x.io != 0 && tq == org_tq-1)
			{
				/* print to file io-interrupt action */
				PRINT_STATE(env.outfs, Clock, x, IO_INTER);
				if (env.interactive)
					PRINT_STATE_INTER(std::cout, Clock, x, IO_INTER);

				x.io_counter = 0;
				io_q.push_back(x);				
				running = false;
			}	
		}

		/* if a process is not running, start one that's available */
		if (!running && !ready_age_q.empty())
		{
			/****************** GET NEXT PROCESS ****************/
			if (env.scheduler == EDF) /* edf scheduler */
			{
				/* find process which can meet its deadline */
				for (x=ready_age_q.top(); 
						 Clock+x.bst>x.dline;
						 x=ready_age_q.top())
				{
					wait[x.pid] = 0;
					if (env.interactive)
						PRINT_STATE_INTER(std::cout, Clock, x, ABORT);
					ready_age_q.pop();
					stats.np--;
					if (ready_age_q.empty())
						break;
				}

				/* we found a process to run. Remove it from the ready queue. */
				if (!ready_age_q.empty())
					ready_age_q.pop();
			} 
			else /* non-real time scheduler */
			{
				x = ready_age_q.top(); 
				ready_age_q.pop();
			}

			/***************** SET UP RUNTIME *******************/
			org_tq = ISKERNEL(x.pri)? env.kernel_tq: env.user_tq;
			tq = 0;
			running = true;				
			
			PRINT_STATE(env.outfs, Clock, x, GETS_CPU);		
			if (env.interactive)
				PRINT_STATE_INTER(std::cout, Clock, x, GETS_CPU);		
		}

		/* print if in interactive mode */
		if (env.interactive)
		{
			print_states(new_q, ready_age_q, io_q, x, running, org_tq-tq, env);
			INTERACTIVE_WAIT(std::cin, line);			
		}

		/* update wait time */
		if (env.scheduler == EDF)
		{ // need vector of values because we drop those that are aborted.
			for (auto iter=ready_age_q.ready_q.begin(); iter!=ready_age_q.ready_q.end(); iter++)
				wait[iter->pid]++;
		}
		else
			stats.awt += (double) ready_age_q.size();
	}

	/* if edf, add all the wait times */
	if (env.scheduler == EDF)
		for (size_t n : wait)
			if (n != 0)
				stats.np++;
}


/************************************************************************/
/*! This functions performs one iteration of io. If a process finishes
		its io it is moved to the ready_age_q.
    \param ready_age_q is the queue of processes organized by scheduler
    							 		 with a synchronized age queue
    \param io_q is the list of processes currently doing io
    \param env is the struct containing user-defined values
*/
/************************************************************************/ 
template<typename T>
void do_io(ready_age_t<T>& ready_age_q, io_t& io_q, env_t& env, size_t Clock)
{
	io_t::iterator iter;
	
	/* iterate through each element */
	for (iter=io_q.begin(); iter!=io_q.end();)
	{
		/* update the io counter and check if io is completed */		
		iter->io_counter++;
		if (iter->io_counter == iter->io) // io has completed
		{
			/* update priority */
			promote_priority(*iter, IO, env);
			
			/* add to ready queue and remove from io queue */
			if (ISMAXED(iter->pri))
				iter->Clock = CLOCK_LAST;
			else
				iter->Clock = Clock; // update the clock when the process is pushed
			ready_age_q.push(*iter);
			
			auto iter_next(++iter);
			io_q.erase(--iter);

			/* check if we removed the last element of the io queue */
			if (io_q.empty()) // otherwise iter!=io_q.end() calls error
				break;

			iter = iter_next;
		}
		else
			iter++;
	}
}


/************************************************************************/
/*! This functions ages each process in the ready queue. If the process
		has aged, its priority is promoted.
    \param ready_age_q is the queue of processes organized by scheduler
    \param env is the struct containing user-defined values
    \param Clock is the current time of execution
*/
/************************************************************************/ 
template<typename T>
void do_aging(ready_age_t<T>& ready_age_q, env_t& env, size_t Clock)
{
	/* for each element in the ready queue */
	for (auto iter_age = ready_age_q.age_q.begin();
		 	iter_age != ready_age_q.age_q.end();
		 	iter_age = ready_age_q.age_q.begin())
	{
		int pri = (*iter_age)->pri;
		if (pri==49 || pri==99)
			break;
		else if (Clock-(*iter_age)->Clock == env.age_time)
		{
			auto iter_ready = ready_age_q.ready_q.search(*(*iter_age));

			if (env.interactive && env.scheduler==PRIORITY)
				PRINT_STATE_INTER(std::cout, Clock, *iter_ready, AGED);

			PCB x = *iter_ready;
			ready_age_q.ready_q.erase(iter_ready);
			ready_age_q.age_q.pop();

			promote_priority(x, AGE, env);
			if (ISMAXED(x.pri))
				x.Clock = CLOCK_LAST;
			else
				x.Clock = Clock;

			ready_age_q.push(x);
		}
		else
			break;
	}
}


/************************************************************************/
/*! This functions demotes the priority of clock-interrupted process.
		The priority is decremented by the time-quantum corresponding to the
		type of process (user or kernel). Arithmetic is restricted to [0,49]
		for user processes and to [50,99] for kernel processes.
    \param x is the current process to be demoted
*/
/************************************************************************/    
void demote_priority(PCB &x, env_t& env)
{
	int& pri = x.pri;
	if (ISKERNEL(pri)) // kernel process
	{ 
		pri -= env.kernel_tq;
		if (pri < 50)
			pri = 50;
	}
	else // user process
	{
		pri -= env.user_tq;
		if (pri < 0)
			pri = 0;
	}
}


/************************************************************************/
/*! This functions promotes the priority of IO/aging process.
		If IO, the priority is incremented corresponding to the io-number.
		If AGE, the priority is incremented corresponding to age_value. 
		type of process (user or kernel). Arithmetic is restricted to [0,49]
		for user processes and to [50,99] for kernel processes.
    \param x is the current process to be demoted
    \param op is type of operation {IO, AGE}
    \param env is the struct of user-defined input values
*/
/************************************************************************/ 
void promote_priority(PCB &x, ops_t op, env_t& env)
{
	int old_pri = x.pri;
	int& pri = x.pri += op==IO? x.io: env.age_val;

	if (ISKERNEL(old_pri)) // kernel process
	{
		if (pri >= 99)
			pri = 99;
	}
	else if (pri >= 49) // user process
		pri = 49;
}


/***********************************************************************/
/*! This functions transfers arriving processes from new_q to ready_age_q
    \param new_q is the queue of processes organized by arrival time
    \param ready_age_q is the queue of processes organized by scheduler
    \param Clock is the current clock-tick
*/
/***********************************************************************/    
template<typename T>
void update(new_t& new_q, ready_age_t<T>& ready_age_q, size_t Clock)
{
	/* following assertion is assumed */
	assert(!new_q.empty()); // new_q is not empty

	/* for each of the earliest elements in new_q, check if it has arrived yet */
	for (PCB x = new_q.top(); x.arr == Clock; x = new_q.top())
	{
		if (ISMAXED(x.pri))
			x.Clock = CLOCK_LAST;
		else
			x.Clock = Clock; // the time the process arrived
		ready_age_q.push(x);
		new_q.pop();
		if (new_q.empty())
			break;
	}
}


/***********************************************************************/
/*! This functions parses the user-provided input and stored in an env_t.
		For any missing values or invalid ones, default values are used.
		\param argc is the first argument of main()
		\param argv is the second argument of main()
		\param env is the env_t obj which is written to by the function.
*/
/***********************************************************************/
void parse_input(int argc, char**argv, env_t& env)
{
	CLParser parser(argc, argv);

	/* display the help command */
	if (parser.optionExists("-h") || parser.optionExists("--help"))
	{
		std::cout << help_output << std::endl;
		exit(0);
	}

	/* get the file name */
	if (parser.optionExists("--generate_processes"))
	{
		int how_many = std::atoi(parser.optionValue("--generate_processes").c_str());
		if (how_many <= 0)
			how_many = 10;

		generate_test_cases(how_many, true);
		env.file_name = "test_cases";
	}
	else if (parser.optionExists("--file_name"))
		env.file_name = parser.optionValue("--file_name");
	else
	{
		std::cerr << "No process file name provided" << std::endl;		
		exit(EXIT_FAILURE);
	}	

	/* get time quantum for kernel level processes  */
	if (parser.optionExists("--kernel_quantum"))
		env.kernel_tq = std::atoi(parser.optionValue("--kernel_quantum").c_str());
	else
		env.kernel_tq = DEFAULT_KERNEL_QUANTUM;
	if (env.kernel_tq <= 0)
		env.kernel_tq = DEFAULT_KERNEL_QUANTUM;

	/* get time quantum for user level processes */
	if (parser.optionExists("--user_quantum"))
		env.user_tq = std::atoi(parser.optionValue("--user_quantum").c_str());
	else
		env.user_tq = DEFAULT_USER_QUANTUM;
	if (env.user_tq <= 0)
		env.user_tq = DEFAULT_USER_QUANTUM;

	/* get the age timer */
	if (parser.optionExists("--age_timer"))
		env.age_time = std::atoi(parser.optionValue("--age_timer").c_str());
	else
		env.age_time = DEFAULT_AGE_TIME;

	/* get the amount to age a process when its age time expires */
	if (parser.optionExists("--age_amount"))
		env.age_val = std::atoi(parser.optionValue("--age_amount").c_str());
	else
		env.age_val = DEFAULT_AGE_VALUE;

	/* get the scheduler type */
	if (parser.optionExists("--scheduler"))
	{
		std::string scheduler = parser.optionValue("--scheduler");
		std::transform(scheduler.begin(), scheduler.end(), scheduler.begin(), ::toupper);
		if (scheduler.compare("FIFO") == 0)
			env.scheduler = FIFO;
		else if (scheduler.compare("SJF") == 0)
			env.scheduler = SJF;
		else if (scheduler.compare("PRIORITY") == 0)
			env.scheduler = PRIORITY;
		else if (scheduler.compare("EDF") == 0)
			env.scheduler = EDF;
		else
		{
			std::cerr << "The scheduler \'" << scheduler << "\' is invalid." << std::endl;
			std::exit(1);
		}
	}
	else
		env.scheduler = DEFAULT_SCHEDULER;

	/* check if interactive */
	env.interactive = parser.optionExists("--interactive");

	/* open the output file */
	std::string output_fn = std::string("output-") + env.file_name;
	std::replace(output_fn.begin(), output_fn.end(), '/', '-');
	std::replace(output_fn.begin(), output_fn.end(), '\\', '-');
	env.outfs.open(output_fn.c_str());

#ifdef _DEBUG
	env.print();
#endif
}

std::string help_output = 
"Usage: ./main [OPTION(s)]\n\n"
"Help:\n"
"  -h, --help\t\t\t\tdisplay this menu\n\n"
"Mandatory arguments: only need one (ordered by precedence)\n"
"  --generate_processes=<how-many>\tautomatically generates test cases\n"
"  --file_name=<file-name>\t\tname of file with processes\n\n"
"Optional arguments:\n"
"  --age_amount=<age-amount>\t\tamount to increase priority after aging\n"
"  --age_timer=<age-timer>\t\ttime to age\n"
"  --interactive\t\t\t\topen interactive shell\n"
"  --kernel_quantum=<kernel-quantum>\ttime quantum for kernel processes\n"
"  --scheduler=<{fifo,sjf,priority,edf}> the process scheduler algorithm to use\n"
"  --user_quantum=<user-quantum>\t\ttime quantum for user processes\n\n"
"Author: Sanfer D\'souza\n"
"e-mail: dsouz039@umn.edu";


/***********************************************************************/
/*! This functions prints the output during interactive running.
		\param new_q is the new queue
		\param ready_age_q is the ready_q following a scheduler protocol
											 synchronized with a age_q.
		\param io_q is the queue of all I/O processes (wait queue)
		\param x is the running process if running=true and nonesense otherwise
		\param running is a flag to identify if x is running or meaningless
*/
/***********************************************************************/
template<typename T>
void print_states(new_t& new_q, ready_age_t<T>& ready_age_q, io_t& io_q, PCB x, bool running, int tq, env_t& env)
{

	/***********************************************************************
	 ********************************* TYPES *******************************
	 ***********************************************************************/

	enum state_t 
	{
		NEW,
		READY,
		IO,
		RUNNING
	};

	struct pair_t 
	{
		PCB x;
		state_t state;
	};

	class mycmp_pid 
	{
	public:
		int operator() (const pair_t& lhs, const pair_t& rhs) const
		{
			return rhs.x.pid - lhs.x.pid;
		}
	};

	struct functor 
	{
		static std::string converter(state_t a)
		{
			switch (a)
			{
				case NEW:
					return "new";
				case READY:
					return "ready";
				case IO:
					return "io";
				case RUNNING:
					return "running";
			}
		}
	};

	typedef priority_queue<pair_t, mycmp_pid> pq_t;
	pq_t io_vec, vec;

	/***********************************************************************
	 ****************************** CATEGORIZE *****************************
	 ***********************************************************************/

	if (running)
		vec.push({.x=x, .state=RUNNING});

	for (auto iter=new_q.begin(); iter!=new_q.end(); iter++)
		vec.push({.x=*iter, .state=NEW});

	for (auto iter=ready_age_q.ready_q.begin(); iter!=ready_age_q.ready_q.end(); iter++)
		vec.push({.x=*iter, .state=READY});

	for (auto iter=io_q.begin(); iter!=io_q.end(); iter++)
		vec.push({.x=*iter, .state=IO});

	static int size;
	if (size==0)
	 	size = new_q.size() + ready_age_q.ready_q.size() + 1 + (running==true);
	
	std::vector<bool> end;
	for (int i=0; i!=size; end.push_back(true), ++i); end[0]=false;
	for (auto iter=vec.begin(); iter!=vec.end(); end[iter++->x.pid]=false);

	/***********************************************************************
	 ********************************* PRINT *******************************
	 ***********************************************************************/

	std::cout << std::endl;

	std::cout << "RUNNING:\t";
	if (running)
		std::cout << "pid: " << x.pid << " (tq: " << tq << ")";
	else
		std::cout << "none";
	std::cout << std::endl;

	std::cout << "TERMINATED:";
	for (auto iter=end.begin(); iter!=end.end(); iter++)
		if (*iter)
			std::cout << "\t" << iter-end.begin();
	std::cout << std::endl;

	std::cout << "IO-QUEUE:";
	for (auto iter=vec.begin(); iter!=vec.end(); iter++)
		if (iter->state == IO)
			std::cout << "\t" << iter->x.pid << "(" << iter->x.io-iter->x.io_counter << ")";
	std::cout << std::endl;
	std::cout << std::endl;

	std::cout << "STATE:\t";
	for (auto iter=vec.begin(); iter!=vec.end(); iter++)
		std::cout << "\t" << functor::converter(iter->state);
	std::cout << std::endl;

	std::cout << "PID:\t";
	for (auto iter=vec.begin(); iter!=vec.end(); iter++)
		std::cout << "\t" << iter->x.pid << "(" << (ISKERNEL(iter->x.pri)? 'k': 'u') << ")";
	std::cout << std::endl;

	if (env.scheduler == PRIORITY)
	{
		std::cout << "PRIORITY:";
		for (auto iter=vec.begin(); iter!=vec.end(); iter++)
			std::cout << "\t" << iter->x.pri;
		std::cout << std::endl;	
	}

	std::cout << "BURST:\t";
	for (auto iter=vec.begin(); iter!=vec.end(); iter++)
		std::cout << "\t" << iter->x.bst;
	std::cout << std::endl;

	std::cout << "ARRIVAL:";
	for (auto iter=vec.begin(); iter!=vec.end(); iter++)
		std::cout << "\t" << iter->x.arr;
	std::cout << std::endl;

	if (env.scheduler == PRIORITY)
	{
		std::cout << "Clock:\t";
		for (auto iter=vec.begin(); iter!=vec.end(); iter++)
		{
			if (iter->x.Clock == CLOCK_LAST)
				std::cout << "\t" << (int) -1;
			else
				std::cout << "\t" << iter->x.Clock;
		}
		std::cout << std::endl;
	}

	if (env.scheduler != EDF)
	{
		std::cout << "IO:\t";
		for (auto iter=vec.begin(); iter!=vec.end(); iter++)
			std::cout << "\t" << iter->x.io;
		std::cout << std::endl;
	}

	if (env.scheduler == EDF)
	{
		std::cout << "DLINE:\t";
		for (auto iter=vec.begin(); iter!=vec.end(); iter++)
			std::cout << "\t" << iter->x.dline;
		std::cout << std::endl;
	}

	std::cout << std::endl;
}
#ifndef PROC_QUEUES_H
#define PROC_QUEUES_H

#include <algorithm>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <list>
#include <string>
#include <vector>

#include "PCB.h"
#include "priority_queue.h"

/**************************************************************
 ************************* COMPARATORS ************************
 *************************************************************/

/* comparator object to order the new queue on arrival times.
 * ties are broken with PID
 * \ret +ve if lhs pops before rhs; -ve if rhs pops before lhs
 */
class mycmp_new
{
public:
	int operator() (const PCB& lhs, const PCB &rhs) const
	{
		if (lhs.arr != rhs.arr)
			return rhs.arr - lhs.arr;
		else
			return rhs.pid - lhs.pid;
	}
};

/* comparator object to order the priority queue.
 * Larger priority number implies higher priority
 * 0-49 = user processes
 * 50-99 = kernel processes
 * Ties are broken with order of arrival
 * \ret +ve if lhs pops before rhs; -ve if rhs pops before lhs
 */
class mycmp_priority
{
public:
	int operator() (const PCB& lhs, const PCB& rhs) const
	{
		if (lhs.pri != rhs.pri)
			return lhs.pri - rhs.pri;
		else if (rhs.Clock != lhs.Clock)
			return rhs.Clock - lhs.Clock;
		else
			return rhs.pid - lhs.pid;
	}
};

/* comparator object to order the priority queue in FIFO order 
 * \ret +ve if lhs pops before rhs; -ve if rhs pops before lhs
 */
class mycmp_fifo
{
public:
	int operator() (const PCB& lhs, const PCB& rhs) const
	{
		if (rhs.Clock != lhs.Clock)
			return rhs.Clock - lhs.Clock;
		else
			return rhs.pid - lhs.pid;		
	}
};

/* comparator object to order the priority queue in SJF order 
 * \ret +ve if lhs pops before rhs; -ve if rhs pops before lhs
 */
class mycmp_sjf
{
public:
	int operator() (const PCB& lhs, const PCB& rhs) const
	{
		if (lhs.bst != rhs.bst)
			return rhs.bst - lhs.bst;
		else if (rhs.Clock != lhs.Clock)
			return rhs.Clock - lhs.Clock;
		else
			return rhs.pid - lhs.pid;
	}
};

/* comparator object to order the priority queue in EDF order */
class mycmp_edf
{
public:
	int operator() (const PCB& lhs, const PCB& rhs) const
	{
		if (lhs.dline != rhs.dline)
			return rhs.dline - lhs.dline;
		else
			return rhs.pid - lhs.pid;
	}
};

/* comparator object for age_t. The front of the age_q has the
 * earliest Clock time. If clock times are tied, then broken
 * by priority. The important point is that user processes with
 * priority 49 and kernel processes with priority 99 will always
 * be popped only after all other processes have been popped.
 * \ret +ve if lhs pops before rhs; -ve if rhs pops before lhs
 */
template<typename T>
class mycmp_age
{
public:
	int operator() (const T& lhs, const T& rhs) const
	{
		if (lhs->Clock != rhs->Clock)
			return rhs->Clock - lhs->Clock;
		else if (lhs->pri != rhs->pri)
		{ 
			/* map 0-49 to [0,99] (even) 
			 * map 50-99 to [0,99] (odd)
			 * preserve order.
			 * This way the last node has original pri=99 and new pri=99
			 * and second-last has original pri=49 and new pri=98, etc
			 */
			int lpri, rpri;

			lpri = lhs->pri;
			if (lpri < 50)
				lpri *= 2;
			else
				lpri = (lpri-50)*2 + 1;

			rpri = rhs->pri;
			if (rpri < 50)
				rpri *= 2;
			else
				rpri = (rpri-50)*2 + 1;

			return rpri - lpri;
		}
		else
			return rhs->pid - lhs->pid;
	}
};


/*******************************************************************
 ******************************* TYPES *****************************
 ******************************************************************/
typedef priority_queue<PCB, mycmp_new> new_t;
typedef priority_queue<PCB, mycmp_priority> priority_t;
typedef priority_queue<PCB, mycmp_fifo> fifo_t;
typedef priority_queue<PCB, mycmp_sjf> sjf_t;
typedef priority_queue<PCB, mycmp_edf> edf_t;
typedef std::list<PCB> io_t;

template<typename T> 
using age_t = priority_queue<T, mycmp_age<T> >;

/********************************************************************
 ***************************** FUNCTIONS ****************************
 *******************************************************************/

/*******************************************************************/
/*! This function returns the new_q from the process file.
		\param fname is the name of the file with the proceses
		\retrun the queue of processes ordered by arrival time. Ties
						broken by PID.
*/
/*******************************************************************/
new_t create_new_queue(std::string fname)
{
	#define VALID(x) (x.pid>0 && x.bst>0 && x.arr>=0 && x.pri>=0 && x.pri<=99 && x.dline>0 && x.io>=0)

	new_t new_q;
	std::ifstream infile;
	std::string line;

	/* open the file */
	infile.open(fname.c_str());
	if (!infile.is_open())
	{
		std::cerr << "Could not read file \'" << fname << "\'" << std::endl;
		exit(EXIT_FAILURE);
	}

	/* get the first line of titles */
	std::getline(infile, line);

#ifdef _DEBUG
	int total_burst=0;
#endif

	/* parse */
	while(std::getline(infile, line))
	{
		PCB x;
		if (std::count(line.begin(), line.end(), '\t') == 5) /* avoid incomplete lines */
		{
			sscanf(line.c_str(), 
						"%i\t%i\t%i\t%i\t%i\t%i\n", 
						&x.pid, 
						&x.bst, 
						&x.arr, 
						&x.pri,
						&x.dline,
						&x.io);
			x.Clock = 0;
			if (VALID(x))
				new_q.push(x);

#ifdef _DEBUG
			if (VALID(x))
				total_burst += x.bst;
#endif			
		}
	}

#ifdef _DEBUG
	std::cout << "Total Execution Time: " << total_burst << std::endl;
#endif

	infile.close();
	return new_q;
}

void generate_test_cases(int how_many, bool has_io)
{
	std::ofstream testfs("test_cases");
	++how_many;

	srand(time(NULL));

	testfs << "Pid\t" << "Bst\t" << "Arr\t" << "Pri\t" << "Dline\t" << "IO" << std::endl; 
	for (int pid=1; pid!=how_many; pid++)
	{
		int bst = 0; while(bst==0) bst=rand()%20;
		int arr = rand()%how_many;
		int pri = rand()%100;
		int dline = rand()%99 + 1;
		int io;
		if (has_io && rand()%100<50)		
			io = rand()%25;
		else io = 0;

		testfs << pid << "\t" << bst << "\t" << arr << "\t" << pri << "\t";
		testfs << dline << "\t" << io << std::endl;
	}

	testfs.close();
}

#endif
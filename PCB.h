#ifndef PCB_H
#define PCB_H

#include <iostream>

class PCB {
public:
	int pid; // PID
	int bst; // Burst Time
	int arr; // Arrival Time
	int pri; // Priority
	int dline; // Deadline
	int io; // IO time

	int io_counter; // IO counter: auxiliary for doing IO
	size_t Clock; // time when the process was pushed in the ready queue: ageing

	void print()
	{
		std::cerr << "pid: " << pid << std::endl;
		std::cerr << "bst: " << bst << std::endl;
		std::cerr << "arr: " << arr << std::endl;
		std::cerr << "pri: " << pri << std::endl;
		std::cerr << "dline: " << dline << std::endl;
		std::cerr << "io: " << io << std::endl;
		std::cerr << "Clock: " << Clock << std::endl;
		std::cerr << std::endl;
	}
};

#endif
# ProcessScheduleSimulation

Process scheduler.
Written in C and tested on Linux.
Should be generic so replace gcc with your own compiler choice.
To build: `make`
To run: `./main --interactive --file_name=test-cases-10 --user_quantum=5 --kernel_quantum=5 --age_amout=5`
To see options: `./main --help`

Note the `--interactive` flag shows the terminal UI. Otherwise the scheduler runs and only shows stats upon termination.

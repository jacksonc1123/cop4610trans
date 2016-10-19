# Trans

A project for my Operating Systems class.

Saving it for future use/experimentation

## How it works:

Creates a parent and child process, and some shared memory, and a 
bidirectional pipe (two unidirectional pipes). The program reads a file into
shared memory 4KB at a time, and sends a confirmation message to the child
process through the pipe. The child writes the data in shared memory to an
output file and sends acknowledgement back through the pipe. 
